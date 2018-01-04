/*
 * sensing_module.h
 *
 *  Created on: Dec 8, 2016
 *      Author: tiago
 */

#ifndef LINUX_SENSING_MODULE_H_
#define LINUX_SENSING_MODULE_H_

#include "../sensed_data.h"

class LinuxSensingModule
{
  private:

	static bool _attached;

	int _module_file_if;
	void* _module_shared_mem_raw_ptr;
	volatile bool _sensingRunning;
	SensedData _sensed_data;

  public:
	LinuxSensingModule();

	~LinuxSensingModule();

	//disconnects this object from the module without checks
	//will make this object invalid
	void forceDetach();

  public:

	void sensingStart();
	void sensingStop();

	bool isSensing() { return _sensingRunning; }

	int createSensingWindow(int period_ms);

	int nextSensingWindow();

	void resgisterAsDaemonProc();//registers calling process as a daemon process
	bool unresgisterAsDaemonProc();

	const SensedData& data() { return _sensed_data; }

	//Returns true if counter is being collected.
	bool isPerfCntAvailable(perfcnt_t cnt);

	void enablePerTaskSensing();
	void pinAllTasksToCPU(int cpu);

	void tracePerfCounter(perfcnt_t perfcnt);
	void tracePerfCounterResetAll();

	void cleanUpCreatedTasks();
};

#endif /* SENSING_MODULE_H_ */