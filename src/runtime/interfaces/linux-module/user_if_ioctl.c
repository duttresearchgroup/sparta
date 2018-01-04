
/*
 * Implementation of IOCTL COMMANDS
 */



#include "../linux-module/core.h"
#include "../linux-module/helpers.h"
#include "../linux-module/sense.h"
#include "../linux-module/sense_data.h"
#include "../linux-module/sensing_window.h"
#include "../linux-module/setup.h"
#include "../linux-module/user_if.h"
#include "../linux-module/user_if_ioctl.h"


CIRCBUF_DEF(sensing_window_ready_queue,32);

void sensing_window_ready(int wid){
	//if(circbuf_push(&sensing_window_ready_queue,wid) < 0){
	//	pinfo("sensing_window_ready_queue full !\n");
	//}
	circbuf_push(&sensing_window_ready_queue,wid);
}

int ioctlcmd_sense_window_wait_any(){
	uint32_t sensing_wid;
	circbuf_pop(&sensing_window_ready_queue,&sensing_wid);
	return sensing_wid;
}

int ioctlcmd_sensing_start(){
	start_sensing_windows();
	pinfo("Sensing started\n");
	return 0;
}

int ioctlcmd_sensing_stop(){
	stop_sensing_windows();
	pinfo("Sensing stoped\n");
	sensing_window_ready(WINDOW_EXIT);
	return 0;
}

int ioctlcmd_sense_window_create(int period_ms){
	return create_sensing_window(period_ms);
}

int ioctlcmd_enable_pertask_sensing(int enable){
	set_per_task_sensing(enable);
	return 0;
}

int ioctlcmd_enable_pintask(int cpu){
	set_pin_task_to_cpu(cpu);
	return 0;
}

int ioctlcmd_perfcnt_enable(int perfcnt){
	if(trace_perf_counter(perfcnt)) return 0;
	else return -1;
}

int ioctlcmd_perfcnt_reset(){
	if(trace_perf_counter_reset()) return 0;
	else return -1;
}

int ioctlcmd_task_beat_updated(pid_t task_pid){
	if(update_task_beat_info(task_pid)) return 0;
	else return -1;
}