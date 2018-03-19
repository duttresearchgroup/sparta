/*******************************************************************************
 * Copyright (C) 2018 Tiago R. Muck <tmuck@uci.edu>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#include "types.h"

const std::string SensingTypeInfo<SEN_PERFCNT>::str        = "perfcnt";

const std::string SensingTypeInfo<SEN_TOTALTIME_S>::str    = "total_time_s";

const std::string SensingTypeInfo<SEN_BUSYTIME_S>::str     = "busy_time_s";

const std::string SensingTypeInfo<SEN_BEATS>::str0         = "beats0";
const std::string SensingTypeInfo<SEN_BEATS>::str1         = "beats1";
const std::string SensingTypeInfo<SEN_BEATS>::str2         = "beats2";
const std::string SensingTypeInfo<SEN_BEATS>::str3         = "beats3";
const std::string SensingTypeInfo<SEN_BEATS>::str4         = "beats_there_shouldnt_be_this_many_domains";

const std::string SensingTypeInfo<SEN_NIVCSW>::str         = "nivcsw";

const std::string SensingTypeInfo<SEN_NVCSW>::str          = "nvcsw";

const std::string SensingTypeInfo<SEN_POWER_W>::str        = "power_w";

const std::string SensingTypeInfo<SEN_TEMP_C>::str         = "temp_c";

const std::string SensingTypeInfo<SEN_FREQ_MHZ>::str       = "freq_mhz";

const std::string SensingTypeInfo<SEN_LASTCPU>::str        = "core";
