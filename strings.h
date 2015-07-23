/*
 * This source file is Copyright 1995 by Evan Scott.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 */

#define MSG_HOTKEY (0)
#define MSG_HOSTS (1)
#define MSG_CANT_FIND_HOSTS (2)
#define MSG_FTPM_STARTUP_ERROR (3)
#define MSG_OK (4)
#define MSG_TCP_HANDLER (5)
#define MSG_SERVICE (6)
#define MSG_CANT_LAUNCH_TCP (7)
#define MSG_LOCAL_HANDLER (8)
#define MSG_CANT_LAUNCH_LOCAL (9)
#define MSG_CONTINUE_EXIT (10)
#define MSG_USER (11)
#define MSG_HOST (12)
#define MSG_USER_NOT_SET (13)
#define MSG_HOST_NOT_SET (14)
#define MSG_USER_HOST_NOT_SET (15)
#define MSG_VOLUME (16)
#define MSG_STATUS_HANDLER (17)
#define MSG_CANT_LAUNCH_STATUS (18)
#define MSG_UNKNOWN (19)
#define MSG_RETRY_MORE_CANCEL (20)
#define MSG_RETRY_CANCEL (21)
#define MSG_MORE_OK (22)
#define MSG_OPERATIONAL_ERROR (23)
#define MSG_OOM_ROOT (24)
#define MSG_PWD_GARBAGE (25)
#define MSG_FAILED_PWD (26)
#define MSG_ERROR_READING_PWD (27)
#define MSG_ERROR_REQUESTING_PWD (28)
#define MSG_ERROR_READING_TYPE (29)
#define MSG_ERROR_SETTING_TYPE (30)
#define MSG_LOGIN_SUCCEEDED_NO_PASS (31)
#define MSG_LOGIN_SUCCEEDED (32)
#define MSG_LOGIN_FAILED (33)
#define MSG_ACCT_REQUESTED (34)
#define MSG_LOGIN_INCORRECT (35)
#define MSG_LOGIN_FAILED_PASS (36)
#define MSG_LOGIN_ERROR (37)
#define MSG_LOST_CONN_DURING_LOGIN_PASS (38)
#define MSG_GARBAGE_RECEIVED_PASS (39)
#define MSG_TEMP_LOGIN_FAILURE_USER (40)
#define MSG_LOGIN_FAILED_USER (41)
#define MSG_LOST_CONN_DURING_LOGIN (42)
#define MSG_GARBAGE_RECEIVED_USER (43)
#define MSG_ERROR_USER_RESPONSE (44)
#define MSG_ERROR_WRITING_PASS (45)
#define MSG_ERROR_WRITING_USER (46)
#define MSG_CONNECT_ERROR (47)
#define MSG_AMITCP_NOT_RUNNING (48)
#define MSG_HOST_UNKNOWN (49)
#define MSG_HOST_UNREACHABLE (50)
#define MSG_FTP_REFUSED (51)
#define MSG_CANT_CONNECT (52)
#define MSG_LOST_CONN_DURING_INTRO (53)
#define MSG_GARBAGE_DURING_INTRO (54)
#define MSG_ERROR_DURING_INTRO (55)
#define MSG_CONN_DELAY (56)
#define MSG_TEMP_CONN_FAILURE (57)
#define MSG_CONN_FAILED (58)
#define MSG_CONNECTING_TO (59)
#define MSG_CONNECTING (60)
#define MSG_LOGIN_TO (61)
#define MSG_USER_NAME (62)
#define MSG_PASSWORD_NAME (63)
#define MSG_CURRENT_SITES (64)
#define MSG_STATE_UNKNOWN (65)
#define MSG_STATE_DISCONNECTED (66)
#define MSG_STATE_CONNECTING (67)
#define MSG_STATE_IDLE (68)
#define MSG_STATE_DISCONNECTING (69)
#define MSG_STATE_LISTING (70)
#define MSG_STATE_CD (71)
#define MSG_STATE_OPENING (72)
#define MSG_STATE_CLOSING (73)
#define MSG_STATE_READING (74)
#define MSG_STATE_WRITING (75)
#define MSG_STATE_LOGIN (76)
#define MSG_STATE_ABORTING (77)
#define MSG_STATE_DELETING (78)
#define MSG_STATE_MAKEDIR (79)
#define MSG_STATE_RENAMING (80)
#define MSG_STATE_PAD1 (81)
#define MSG_STATE_PAD2 (82)
#define MSG_STATE_PAD3 (83)
#define MSG_QUICK_FLAG (84)
#define MSG_BROKER_NAME (85)
#define MSG_BROKER_DESCR (86)
#define MSG_USER_TT (87)
#define MSG_PASSWORD_TT (88)
#define MSG_STATUS_TT (89)
#define MSG_QUICK_TT (90)
#define MSG_HOST_TT (91)
#define MSG_ROOT_TT (92)
#define MSG_CANCEL (93)
#define MSG_ABORT (94)
#define MSG_DISCONNECT (95)
#define MSG_LOGIN (96)
#define MSG_ERROR_RESPONSE_PASS (97)
#define MSG_OFF (98)
#define MSG_FALSE (99)
#define MSG_CASE_TT (100)
#define MSG_SLOW_TT (101)
#define MSG_MESSAGES_TT (102)
#define MSG_ALL (103)
#define MSG_NONE (104)
#define MSG_ERROR (105)
#define MSG_DEFAULT (106)
#define MSG_PORT_TT (107)

#define NUM_MSGS 108

#ifdef DECLARE_GLOBALS_HERE
b8 *strings[] = {
	"ctrl alt f",
	"Hosts",
	"Can't find Hosts dir",
	"FTPMount startup error",
	"Ok",
	"(FTPMount) TCP Handler",
	"ftp",
	"Can't launch TCP handler",
	"(FTPMount) local handler",
	"Can't launch local handler",
	"Continue|Exit",
	"USER",
	"HOST",
	"USER environment variable not set\nAnonymous login with %s\nused as your address",
	"HOST environment variable not set\nAnonymous login with %s\nused as your address",
	"USER and HOST environment variables not set\nAnonymous login with %s\nused as your address",
	"FTPMount",
	"(FTPMount) status handler",
	"Can't launch status handler",
	"Unknown condition (low on memory?)",
	"Retry|More ...|Cancel",
	"Retry|Cancel",
	"More ...|Ok",
	"Operational error",
	"Out of memory for root string",
	"PWD response is unintelligible",
	"Failed to get PWD response",
	"Error reading PWD response",
	"Error while requesting PWD",
	"Error while reading TYPE I response",
	"Error while setting TYPE I",
	"Login succeeded (no password required)",
	"Login succeeded",
	"Login failed",
	"ACCT requested",
	"Login incorrect",
	"Login failed (PASS)",
	"Login error",
	"Lost connection during login (passwd)",
	"Garbage received from remote site (passwd)",
	"Temporary login failure (USER)",
	"Login failed (USER)",
	"Lost connection during login",
	"Garbage received from remote site",
	"Error reading response to login",
	"Error writing PASS",
	"Error writing USER",
	"Connect error",
	"AmiTCP is not running",
	"Host %s is unknown",
	"Host %s is unreachable",
	"FTP connection to %s has been refused",
	"Can't connect to %s (%ld)",
	"Lost connection during intro",
	"Garbage received from %s",
	"Error reading introduction",
	"Connection delay ...",
	"Temporary connection failure ...",
	"Connection failed ...",
	"Connecting to ",
	"Connecting ...",
	"Login to %s",
	"User",
	"Password",
	"Current sites",
	"Unknown",
	"Disconnected",
	"Connecting",
	"Idle",
	"Disconnecting",
	"Listing",
	"Changing Directories",
	"Opening File",
	"Closing File",
	"Reading",
	"Writing",
	"Logging in",
	"Aborting",
	"Deleting",
	"Making Directory",
	"Renaming",
	"Pad 1",
	"Pad 2",
	"Pad 3",
	" (Q) ",
	"FTPMount Status",
	"Status and control of FTPMount",
	"USER",
	"PASSWORD",
	"STATUS",
	"QUICK",
	"HOST",
	"ROOT",
	"Cancel",
	"Abort",
	"Disconnect",
	"Login",
	"Error reading response to password",
	"off",
	"false",
	"CASE",
	"SLOW",
	"MESSAGES",
	"ALL",
	"NONE",
	"ERROR",
	"Default",
	"PORT"
};
#else
extern b8 *strings[NUM_MSGS];
#endif

