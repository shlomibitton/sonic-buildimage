extern "C" {
    #include <sx/sdk/sx_api_init.h>
    #include <sx/sdk/sx_api_host_ifc.h>
    #include <sx/sdk/sx_api_dbg.h>
    #include <sx/sdk/sx_trap_id.h>
    #include <sx/sdk/sx_lib_host_ifc.h>
    #include <sx/sdk/sx_api.h>
}

#include "usock.h"
#include "sdk_fd_wrap.h"

#include <sys/stat.h>
#include <dirent.h>
#include <iostream>
#include <string>
#include <sstream>
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <stdlib.h>
#include <swss/logger.h>
#include <swss/select.h>
#include <thread>
#include <signal.h>

using namespace std;

#define DUMP_COUNTER_LIMIT 10
#define LOG_COUNTER_LIMIT 100
#define SOCKET_TIMEOUT 10
#define DUMP_COUNT_LIMIT_ON_DISK 15
/* Currently CR and SDK, will increase in the future */
#define DUMP_FILES_TYPE 2
static const auto DefaultSocketPath = "/var/run/fw_dump_me/fw.sock";
static string DefaultDumpPath = "/var/log/mellanox/fw_dump_me";

/* Functions */
static void handle_sdk_health_event(sx_health_cause_t cause, sx_health_severity_t severity, sx_trap_id_t trap_id);
static void handle_cli_request(unique_ptr<Connection> conn, swss::Select* select, bool is_running);
void signalHandler(int signum);
void daemon_exit();
int files_count(const char* path);
void delete_oldest_file(char *dir);
string get_time();

/* Global params */
static sx_user_channel_t g_sdk_channel;
static sx_api_handle_t g_sdk_handle;
static int dump_counter = 0;
static bool dump_is_running = false;
static bool fw_event_occur = false;
static bool running = true;

/* Event handler function */
static void handle_sdk_health_event(sx_health_cause_t cause, sx_health_severity_t severity, sx_trap_id_t trap_id) {

    string severity_string, cause_string, current_time, dump_file_path;
    stringstream log_msg_stream;

    /* Assign a proper label for the severity and decide if to take a dump or not */
    switch (severity) {
        case SXD_HEALTH_SEVERITY_CRIT:
        {
            severity_string = "Critical";
            break;
        }
        case SXD_HEALTH_SEVERITY_ERR:
        {
            severity_string = "Error";
            break;
        }
        case SXD_HEALTH_SEVERITY_WARN:
        {
            severity_string = "Warning";
            break;
        }
        case SXD_HEALTH_SEVERITY_NOTICE:
        {
            severity_string = "Notice";
            break;
        }
        default:
        {
            severity_string = "Unknown";
            break;
        }
    } // end switch

    /* Assign a proper label for the cause */
    switch (cause) {
        case SXD_HEALTH_CAUSE_NONE:
        {
            cause_string = "None";
            break;
        }
        case SXD_HEALTH_CAUSE_FW:
        {
            cause_string = "FW health issue";
            break;
        }
        case SXD_HEALTH_CAUSE_GO_BIT:
        {
            cause_string = "go bit not cleared";
            break;
        }
        case SXD_HEALTH_CAUSE_NO_CMDIFC_COMPLETION:
        {
            cause_string = "command interface completion timeout";
            break;
        }
        case SXD_HEALTH_CAUSE_FW_TIMEOUT:
        {
            cause_string = "timeout in FW response";
            break;
        }
        default:
        {
            cause_string = "Unknown";
            break;
        }
    } // end switch

    /* Log the event and limit to one FW cause dump */
    log_msg_stream << "Health event captured, Severity: '" << severity_string << "' Cause: '" << cause_string;
    SWSS_LOG_NOTICE("%s", log_msg_stream.str());

    if (fw_event_occur) {
        dump_is_running = false;
        return;
    }

    if (cause == SXD_HEALTH_CAUSE_FW) {
        fw_event_occur = true;
    }

    sx_status_t sx_err;
    dump_counter++;

    /* Generate FW dump file */
    sx_dbg_extra_info_t dbg_params;
    dbg_params.dev_id = 1;
    dbg_params.is_async = false;
    dbg_params.force_db_refresh = false;
    dbg_params.timeout_usec = 0;
    DefaultDumpPath.copy(dbg_params.path, DefaultDumpPath.length(), 0);
    dbg_params.path[DefaultDumpPath.length()] = '\0';

    sx_err = sx_api_dbg_generate_dump_extra(g_sdk_handle, &dbg_params);
    if (sx_err != SX_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Failed to generate FW dump file %d", sx_err);
    }

    /* Sleep for 1 sec to avoid SDK blocking for too long */
    sleep(1);

    /* Generate SDK dump file */
    current_time = get_time();
    if (current_time == "") {
        SWSS_LOG_ERROR("Failed to get time of day");
    }
    dump_file_path = DefaultDumpPath;
    dump_file_path.append("/sdkdump_").append(current_time).append("\0");
    sx_err = sx_api_dbg_generate_dump(g_sdk_handle, dump_file_path.c_str());
    if (sx_err != SX_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Failed to generate SDK dump file, rc =  %d\n", sx_err);
    }

    /* Check how many dumps exist, if limit reached delete oldest one */
    if (files_count(DefaultDumpPath.c_str()) > (DUMP_COUNT_LIMIT_ON_DISK * DUMP_FILES_TYPE) + 2){
        SWSS_LOG_NOTICE("Dump files count reached maximum allowed, deleteing oldest one...");
        char c_str_dump_path[DefaultDumpPath.length() + 1];
        strncpy(c_str_dump_path, DefaultDumpPath.c_str(), DefaultDumpPath.length());
        c_str_dump_path[DefaultDumpPath.length()] = '\0';
        for (int i = 0; i < DUMP_FILES_TYPE; i++) {
            delete_oldest_file(c_str_dump_path);
        }
    }

    dump_is_running = false;
}

/* CLI handler function */
static void handle_cli_request(unique_ptr<Connection> conn, swss::Select* select, bool is_running) {

    string recvmsg, reply, current_time;
    string dump_file_path = DefaultDumpPath;
    bool result = true;

    /* Path */
    if (!(conn->recv(recvmsg)) || recvmsg.empty())
    {
        SWSS_LOG_ERROR("Failed to get request from CLI");
    }
    if (recvmsg != "None") {
        dump_file_path = recvmsg;
        dump_file_path[recvmsg.length()] = '\0';
    }

    if (is_running) {
        reply = "Generating dump...\nFailed, Another dump task is currently running\n";
    }
    else if (!is_running && fw_event_occur){
        reply = "Generating dump...\nFailed, FW event occured and a dump was already taken\n";
    }

    /* Normal operation */
    else {
        /* Generate FW dump file */
        sx_dbg_extra_info_t dbg_params;
        dbg_params.dev_id = 1;
        dbg_params.is_async = false;
        dbg_params.force_db_refresh = false;
        dbg_params.timeout_usec = 0;
        dump_file_path.copy(dbg_params.path, dump_file_path.length(), 0);
        dbg_params.path[dump_file_path.length()] = '\0';
        sx_status_t sx_err;

        sx_err = sx_api_dbg_generate_dump_extra(g_sdk_handle, &dbg_params);
        if (sx_err != SX_STATUS_SUCCESS) {
            SWSS_LOG_ERROR("Failed to generate FW dump file, rc = %d\n", sx_err);
            result = false;
        }

        /* Sleep for 1 sec to avoid SDK blocking for too long */
        sleep(1);

        /* Generate SDK dump file */
        current_time = get_time();
        if (current_time == "") {
            SWSS_LOG_ERROR("Failed to get time of day");
        }
        dump_file_path.append("/sdkdump_").append(current_time).append("\0");
        sx_err = sx_api_dbg_generate_dump(g_sdk_handle, dump_file_path.c_str());
        if (sx_err != SX_STATUS_SUCCESS) {
            SWSS_LOG_ERROR("Failed to generate SDK dump file, rc =  %d\n", sx_err);
            result = false;
        }

        if (result) {
            reply = "Generating dump...\nFinished successfully\nOutput = ";
            reply.append(dump_file_path).append("\n");
        } else {
            reply = "Generating dump...\nFailed to create FW/SDK dump file(s)\n";
        }
    }
    
    /* Send reply */
    if (!(conn->send(reply)))
    {
        SWSS_LOG_ERROR("Failed to send reply to CLI");
    }
    (*select).removeSelectable(conn.get());
    conn.reset();

    /* Check how many dumps exist, if limit reached delete oldest one */
    if (files_count(DefaultDumpPath.c_str()) > (DUMP_COUNT_LIMIT_ON_DISK * DUMP_FILES_TYPE) + 2){
        SWSS_LOG_NOTICE("Dump files count reached maximum allowed, deleteing oldest one...");
        char c_str_dump_path[DefaultDumpPath.length() + 1];
        strncpy(c_str_dump_path, DefaultDumpPath.c_str(), DefaultDumpPath.length());
        c_str_dump_path[DefaultDumpPath.length()] = '\0';
        for (int i = 0; i < DUMP_FILES_TYPE; i++) {
            delete_oldest_file(c_str_dump_path);
        }
    }

    dump_is_running = false;
}

/* Signal handler override */
void signalHandler(int signum)
{
    SWSS_LOG_ENTER();

    switch(signum)
    {
    case SIGINT:
        SWSS_LOG_NOTICE("Caught SIGINT, exiting ...");
        daemon_exit();
        signal(SIGINT, SIG_DFL);
        break;
    case SIGTERM:
        SWSS_LOG_NOTICE("Caught SIGTERM, exiting ...");
        daemon_exit();
        signal(SIGTERM, SIG_DFL);
        break;
    default:
        SWSS_LOG_NOTICE("Unhandled signal: %d, ignoring ...", signum);
        break;
    }
    
}

/* Gracefull shutdown for the daemon */
void daemon_exit() {
    sx_status_t sx_err;
    /* Close host_ifc */
    sx_err = sx_api_host_ifc_close(g_sdk_handle, &g_sdk_channel.channel.fd);
    if (sx_err != SX_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Failed to close HOST IFC API rc = %d", sx_err);
    }

    /* Close SDK API */
    sx_err = sx_api_close(&g_sdk_handle);
    if (sx_err != SX_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Failed to close SDK API rc = %d", sx_err);
    }
    running = false;
}

/* Get a path and return the amount of files under this directory */
int files_count(const char* path) {
    DIR *dp;
    int i = 0;
    struct dirent *ep;
    dp = opendir(path);
    if (dp != NULL)
    {
        while (ep = readdir (dp))
        {
            i++;
        }
        (void) closedir (dp);
        return i;
    }
    SWSS_LOG_ERROR("Failed to open dump directory");
    return 0;
}

/* Delete oldest file under this directory */
void delete_oldest_file(char *dir) {
    DIR *dp;
    struct dirent *entry, *oldestFile;
    struct stat statbuf;
    time_t t_oldest = 0;

    if((dp = opendir(dir)) != NULL) {
        chdir(dir);
        while((entry = readdir(dp)) != NULL) {
            lstat(entry->d_name, &statbuf);       
            if(strcmp(".",entry->d_name) == 0 || strcmp("..",entry->d_name) == 0){
                continue;
            }
            if(statbuf.st_mtime > t_oldest){
                t_oldest = statbuf.st_mtime;
                oldestFile = entry;
            }
        }
        remove(oldestFile->d_name);
        closedir(dp);
    } else {
        SWSS_LOG_ERROR("Failed to open dump directory");
    }
}

/* Get current time string */
string get_time() {
    char time_str[30] = {0};
    struct timeval tv;
    struct tm *nowtm = NULL;

    gettimeofday(&tv, NULL);
    nowtm = localtime(&tv.tv_sec);
    if (NULL == nowtm) {
        return "";
    }
    strftime(time_str, 30, "%d_%m_%Y-%H_%M_%S", nowtm);
    return string(time_str);
}

int main()
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);
    SWSS_LOG_ENTER();

    int log_counter = 0;

    /* API handlers param */
    g_sdk_handle = SX_API_INVALID_HANDLE;
    sx_status_t                    sx_err = SX_STATUS_SUCCESS;

    /* Health event param */
    sx_receive_info_t              receive_info;
    sx_event_health_notification_t trap_data;
    uint32_t                       trap_data_len = sizeof(trap_data);

    /* Open SDK API */
    sx_err = sx_api_open(NULL, &g_sdk_handle);
    if (sx_err != SX_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Failed to open SDK API, rc = %d", sx_err);
    }

    /* Open host_ifc to receive traps */
    g_sdk_channel.type = SX_USER_CHANNEL_TYPE_FD;
    sx_err = sx_api_host_ifc_open(g_sdk_handle, &g_sdk_channel.channel.fd);
    if (sx_err != SX_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Failed to open HOST IFC API, rc = %d", sx_err);
    }

    /* Register to Health trap group */
    sx_swid_t swid = 0; /* this is an internal SDK event, it is not SWID related */
    sx_err = sx_api_host_ifc_trap_id_register_set(g_sdk_handle, SX_ACCESS_CMD_REGISTER, swid, SX_TRAP_ID_SDK_HEALTH_EVENT, &g_sdk_channel);
    if (sx_err != SX_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Failed to register to SDK health event, rc = %d", sx_err);
    }

    /* Create Unix socket to listen to CLI requests */
    USockSeqPacket usock{DefaultSocketPath};
    unique_ptr<Connection> conn;

    /* Init select params */
    swss::Select select{};
    int rc {swss::Select::ERROR};
    HealthFD sdk_fd = HealthFD(g_sdk_channel.channel.fd.fd);
    select.addSelectable(&usock);
    select.addSelectable(&sdk_fd);

    /* Before starting the main loop, register signals for gracefull shutdown */
    for (auto signum: {SIGINT, SIGTERM})
    {
        if (signal(signum, signalHandler) == SIG_ERR)
        {
            SWSS_LOG_THROW("Failed to set signal handler for signum %d", signum);
        }
    }

    /* trigger a test event which will activate the handler - for dubugging*/
    /*
    sx_dbg_control_params_t params;
    params.dev_id = 1;
    params.enable = true;
    params.trigger_test = true;
    SWSS_LOG_NOTICE("Sending debug control Enable+Test");
    sx_err = sx_api_dbg_control(g_sdk_handle, &params);
    if (sx_err != SX_STATUS_SUCCESS){
        SWSS_LOG_ERROR("Failed to send debug control Enable+Test, rc = %d", sx_err);
    }
    */

    //Main loop
    SWSS_LOG_NOTICE("Main loop started, listening to events...");
    while (running) {
        /* Wait for event */
        swss::Selectable* currentSelectable {nullptr};
        rc = select.select(&currentSelectable);

        if (rc == swss::Select::ERROR)
        {
            SWSS_LOG_ERROR("Select returned error %d", rc);
        }

        else if (rc == swss::Select::OBJECT)
        {
            /* Incoming FW/SDK event */
            if (currentSelectable == &sdk_fd)
            {
                /* Limit number of log entries possible and avoid logging/taking dump if it is already running */
                if ((log_counter >= LOG_COUNTER_LIMIT) || dump_is_running) {
                    continue;
                }
                sx_err = sx_lib_host_ifc_recv(&g_sdk_channel.channel.fd, &trap_data, &trap_data_len, &receive_info);
                if (sx_err != SX_STATUS_SUCCESS) {
                    SWSS_LOG_ERROR("Failed to recieve event data from HOST IFC, rc = %d", sx_err);
                } else {
                    /* Make sure only one dump task is running */
                    dump_is_running = true;
                    /* open new thread to process the event and increment the log counter */
                    log_counter++;
                    thread t (handle_sdk_health_event, trap_data.cause, trap_data.severity, receive_info.trap_id);
                    t.detach();
                }
            }

            /* Incoming CLI connection */
            else if (currentSelectable == &usock)
            {
                if (conn)
                    {
                        select.removeSelectable(conn.get());
                    }
                    conn = usock.accept();
                    conn->setTimeout(SOCKET_TIMEOUT);
                    select.addSelectable(conn.get());
            }

            /* Incoming CLI request */
            else if (conn && currentSelectable == conn.get())
            {
                SWSS_LOG_NOTICE("Dump requested by the user");
                /* Make sure only one dump task is running */
                if(dump_is_running)
                {
                    thread t (handle_cli_request, move(conn), &select, true);
                    t.detach();
                }
                else
                {
                    dump_is_running = true;
                    thread t (handle_cli_request, move(conn), &select, false);
                    t.detach();
                }
            }
        } // end OBJECT
    } // end while

    return EXIT_SUCCESS;
}
