/*===========================================================================

 Copyright (c) 2013 Qualcomm Technologies, Inc.  All Rights Reserved.
 Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/
#ifndef __THERMAL_SERVER_H__
#define __THERMAL_SERVER_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_THERMAL_SERVER
int thermal_server_init(void);
int thermal_server_notify_clients(char *client_name, int level);
int thermal_server_config_info_to_client(void *data);
int thermal_server_register_client_req_handler(char *, int (*callback)(int, void *, void *), void *data);
void thermal_server_unregister_client_req_handler(int client_cb_handle);
void thermal_server_release(void);
#else
static inline int thermal_server_init(void)
{
	return 0;
}
static inline int thermal_server_notify_clients(char *client_name, int level)
{
	return 0;
}
static inline int thermal_server_config_info_to_client(void *data)
{
	return 0;
}
static inline int thermal_server_register_client_req_handler(char *client_name, int (*callback)(int, void *, void *), void *data)
{
	return 0;
}
static inline void thermal_server_unregister_client_req_handler(int client_cb_handle){}
static inline void thermal_server_release(void){}
#endif

#ifdef __cplusplus
}
#endif

#endif /* __THERMAL_SERVER_H__ */

