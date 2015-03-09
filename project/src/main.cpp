/*
 * pstate-frequency Easier control of the Intel p-state driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * For questions please contact pyamsoft at pyam.soft@gmail.com
 */

#include <iostream>
#include <sstream>

#include <getopt.h>
#include <unistd.h>

#include "include/psfreq_color.h"
#include "include/psfreq_cpu.h"
#include "include/psfreq_log.h"
#include "include/psfreq_util.h"
#include "include/psfreq_values.h"

bool setCpuValues(const psfreq::Cpu &cpu, const psfreq::Values &cpuValues);
void printCpuValues(const psfreq::Cpu& cpu);
void printRealtimeFrequency(const psfreq::Cpu& cpu);
void printGPL();
void printVersion();
void printHelp();
int handleOptionResult(const psfreq::Cpu &cpu, psfreq::Values &cpuValues,
		const int result);
int parseOptions(const int argc, char **const argv,
		const psfreq::Cpu &cpu, psfreq::Values &cpuValues,
		const char *const shortOptions,
		const struct option longOptions[]);
int planFromOptArg(char *const arg);
const std::string governorFromOptArg(char *const arg,
		const std::vector<std::string> &availableGovernors);

/*
 * Retrieves the values requested by the user and makes sure that they are
 * sane, or sanitizes them. Then attempts to set the values requested by the
 * user.
 */
bool setCpuValues(const psfreq::Cpu &cpu, const psfreq::Values &cpuValues)
{
	/*
	 * Retrieve the system constant values including the
	 * full range of available CPU frequencies
	 */
	const int cpuInfoMin = cpu.getInfoMinValue();
	const int cpuInfoMax =  cpu.getInfoMaxValue();
	const int cpuMinPstate = cpu.getMinValue();
	const int cpuMaxPstate = cpu.getMaxValue();
	const std::string cpuGovernor = cpu.getGovernor();

	/*
	 * Check for sane system, if any of these values are not stable,
	 * exit immediately, this system is not supported by
	 * pstate-frequency
	 */
	if (cpuInfoMin == 1 || cpuInfoMax == 1
			|| cpuMinPstate == 0 || cpuMaxPstate == 0
			|| cpuGovernor == std::string()) {
		return false;
	} else {
		/*
		 * Sanitize the minimum CPU frequency so that
		 * it can safely be set.
		 */
		const int requestedMin = cpuValues.getMin();
		int newMin = (requestedMin >= 0
				? requestedMin
				: cpuMinPstate);
		newMin = psfreq::boundValue(newMin, cpuInfoMin,
				cpuInfoMax - 1);

		/*
		 * Sanitize the maximum CPU frequency, including
		 * the condition that it be greater than the
		 * minimum, so that it can safely be set.
		 */
		const int requestedMax = cpuValues.getMax();
		int newMax = (requestedMax >= 0
				? requestedMax
				: cpuMaxPstate);
		newMax = psfreq::boundValue(newMax, cpuInfoMin + 1,
				cpuInfoMax);
		/*
		 * Make sure that the minimum frequency is not going
		 * to be greater than or equal to the maximum frequency
		 */
		newMin = (newMin >= newMax
				? newMax - 1
				: newMin);


		/*
		 * If the new maximum frequency that is requested
		 * is less than the current minimum, we must
		 * modify the minimum first before we can actually
		 * change the max frequency
		 */
		if (cpuMinPstate > newMax) {
			cpu.setScalingMin(newMin);
			cpu.setScalingMax(newMax);
		} else {
			cpu.setScalingMax(newMax);
			cpu.setScalingMin(newMin);
		}
		/*
		 * If the system supports a Turbo Boost
		 * type functionality, attempt to set it
		 * as well.
		 */
		const int cpuTurbo = cpu.getTurboBoost();
		if (cpuTurbo != -2) {
			const int turbo = cpuValues.getTurbo();
			int newTurbo = (turbo != -1 ? turbo : cpuTurbo);
			newTurbo = psfreq::boundValue(newTurbo, 0, 1);
			cpu.setTurboBoost(newTurbo);
		}

		/*
		 * Set the software CPU governor
		 */
		const std::string requestedGovernor = cpuValues.getGovernor();
		const std::string newGovernor =
				(requestedGovernor != std::string()
				? requestedGovernor
				: cpuGovernor);
		cpu.setGovernor(newGovernor);
		return true;
	}
}

/*
 * Given the user command line input of either a number or a plan name,
 * decide what the proper power plan to run should be.
 */
int planFromOptArg(char *const arg)
{
	const std::string convertedArg(arg);
	int plan;
	if (convertedArg.compare("1") == 0
			|| psfreq::stringStartsWith("powersave",
				convertedArg)) {
		plan = 1;
	} else if (convertedArg.compare("2") == 0
			|| psfreq::stringStartsWith("performance",
			convertedArg)) {
		plan = 2;
	} else if (convertedArg.compare("3") == 0
			|| psfreq::stringStartsWith("max-performance",
				convertedArg)) {
		plan = 3;
#ifdef INCLUDE_UDEV_RULE
#if INCLUDE_UDEV_RULE == 1
	} else if (convertedArg.compare("0") == 0
		|| psfreq::stringStartsWith("auto", convertedArg)) {
		plan = 0;
#endif
#endif
	} else {
		if (!psfreq::Log::isAllQuiet()) {
			std::cerr << psfreq::Color::boldRed() << "Bad Plan."
				<< psfreq::Color::reset() << std::endl;
		}
		return 4;
	}
	return plan;
}

/*
 * Grab the current CPU frequencies from /proc/cpuinfo
 * and pretty print them to the stdout
 */
void printRealtimeFrequency(const psfreq::Cpu& cpu)
{
	if (psfreq::Log::isOutputCapable()) {
		printVersion();
		const std::vector<std::string> frequencies =
				cpu.getRealtimeFrequencies();
		if (!frequencies.empty()) {
			std::ostringstream oss;
			for (unsigned int i = 0; i < cpu.getNumber(); ++i) {
				std::string freq = frequencies[i];
				oss << psfreq::Color::boldWhite()
					<< "    pstate::"
					<< psfreq::Color::boldGreen()
					<< "CPU["
					<< psfreq::Color::boldMagenta()
					<< i << psfreq::Color::boldGreen()
					<< "]  -> "
					<< psfreq::Color::boldCyan()
					<< freq.substr(0, freq.size() - 1)
					<< "MHz" << psfreq::Color::reset()
					<< std::endl;
			}
			std::cout << oss.str();
		}
	}
}

/*
 * Given the user input which is a governor name, and the list of currently
 * available governors, decide on the governor to set or none if the user
 * requested input is a value which does not exist on the system.
 */
const std::string governorFromOptArg(char *const arg,
		const std::vector<std::string> &availableGovernors)
{
	const std::string convertedArg(arg);
	std::ostringstream gov;
	std::string governor;
	for (unsigned int i = 0; i < availableGovernors.size(); ++i) {
		if (psfreq::stringStartsWith(availableGovernors[i],
				convertedArg)) {
			governor = availableGovernors[i];
			break;
		}
	}
	if (governor == std::string()) {
		if (!psfreq::Log::isAllQuiet()) {
			std::cerr << psfreq::Color::boldRed()
				<< "Bad Governor." << psfreq::Color::reset()
				<< std::endl;
		}
		return std::string();
	}
	return governor;
}

/*
 * Print the first part of the GPL v2 License.
 */
void printGPL()
{
	if (psfreq::Log::isOutputCapable()) {
		std::ostringstream oss;
		oss << "pstate-frequency comes with ABSOLUTELY NO WARRANTY."
			<< std::endl
			<< "This is free software, and you are welcome "
			<< "to redistribute it"
			<< std::endl << "under certain conditions."
			<< std::endl
			<< "Please see the README for details."
			<< psfreq::Color::reset()
			<< std::endl << std::endl;
		std::cout << oss.str();
	}
}

/*
 * Display the current program version if it is set.
 */
void printVersion()
{
	if (psfreq::Log::isOutputCapable()) {
		std::ostringstream oss;
		oss << std::endl;
#ifdef VERSION
		oss << psfreq::Color::boldBlue() << "pstate-frequency  "
			<< psfreq::Color::boldMagenta() << VERSION
			<< psfreq::Color::reset()
			<< std::endl;
#endif
		std::cout << oss.str();
	}
}

/*
 * Print out the current CPU settings as configured either from
 * the cpufreq sysfs files or the intel_pstate sysfs files.
 */
void printCpuValues(const psfreq::Cpu& cpu)
{
	if (psfreq::Log::isOutputCapable()) {
		printVersion();
		std::ostringstream oss;
		oss << psfreq::Color::boldWhite()
			<< "    pstate::" << psfreq::Color::boldGreen()
			<< "CPU_DRIVER     -> "
			<< psfreq::Color::boldCyan() << cpu.getDriver()
			<< std::endl;
		oss << psfreq::Color::boldWhite()
			<< "    pstate::" << psfreq::Color::boldGreen()
			<< "CPU_GOVERNOR   -> "
			<< psfreq::Color::boldCyan() << cpu.getGovernor()
			<< std::endl;
		const int turbo = cpu.getTurboBoost();
		oss << psfreq::Color::boldWhite()
			<< "    pstate::" << psfreq::Color::boldGreen()
			<< (cpu.hasPstate() ? "NO_TURBO       -> "
					: "TURBO_BOOST    -> ")
			<< psfreq::Color::boldCyan() << turbo << " : "
			<< (cpu.hasPstate() ? (turbo == 1 ? "OFF" : "ON")
					: (turbo == 1 ? "ON" : "OFF"))
			<< std::endl;
		oss << psfreq::Color::boldWhite()
			<< "    pstate::" << psfreq::Color::boldGreen()
			<< "CPU_MIN        -> "
			<< psfreq::Color::boldCyan() << cpu.getMinValue()
			<< "% : "
			<< static_cast<int>(cpu.getScalingMinFrequency())
			<< "KHz" << std::endl;
		oss << psfreq::Color::boldWhite()
			<< "    pstate::" << psfreq::Color::boldGreen()
			<< "CPU_MAX        -> "
			<< psfreq::Color::boldCyan() << cpu.getMaxValue()
			<< "% : "
			<< static_cast<int>(cpu.getScalingMaxFrequency())
			<< "KHz" << std::endl;
		oss << psfreq::Color::reset();
		std::cout << oss.str();
	}
}

/*
 * Print the program's options and the intended methods of use.
 */
void printHelp()
{
	if (psfreq::Log::isOutputCapable()) {
		std::ostringstream oss;
		oss << "usage:" << std::endl
			<< "pstate-frequency [verbose] [ACTION] [option(s)]"
			<< std::endl
			<< "verbose:" << std::endl
			<< "    unprivilaged:" << std::endl
			<< "    -d | --debug     Print debugging messages to "
			<< "stdout" << std::endl
			<< "    -q | --quiet     Supress all non-error output"
			<< std::endl
			<< "    -a | --all-quiet Supress all output"
			<< std::endl
			<< "    --color          Colorize output"
			<< std::endl
			<< std::endl
			<< "actions:" << std::endl
			<< "    unprivilaged:" << std::endl
			<< "    -H | --help      Display this help and exit"
			<< std::endl
			<< "    -V | --version   Display application version "
			<< "and exit" << std::endl
			<< "    -G | --get       Access current CPU values"
			<< std::endl
			<< "    privilaged:" << std::endl
			<< "    -S | --set       Modify current CPU values"
			<< std::endl
			<< std::endl
			<< "options:" << std::endl
			<< "    unprivilaged:" << std::endl
			<< "    -c | --current   Display the current user set "
			<< "CPU values" << std::endl
			<< "    -r | --real      Display the real time CPU "
			<< "frequencies" << std::endl
			<< "    privilaged: "<< std::endl
			<< "    -p | --plan      Set a predefined power plan"
			<< std::endl
			<< "    -m | --max       Modify current CPU max "
			<< "frequency" << std::endl
			<< "    -g | --governor  Set the cpufreq governor"
			<< std::endl
			<< "    -n | --min       Modify current CPU min "
			<< "frequency" << std::endl
			<< "    -t | --turbo     Modify curent CPU turbo "
			<< "boost state" << std::endl;
		std::cout << oss.str();
	}
}

/*
 * Given the return value from the getopt_long function as parameter 'result'
 * decide how to handle the option that was entered by the user.
 */
int handleOptionResult(const psfreq::Cpu &cpu, psfreq::Values &cpuValues,
		const int result)
{
	switch(result) {
	case 0:
                return 0;
        case 'H':
		printGPL();
		printHelp();
                return -1;
        case 'c':
		cpuValues.setRequested(0);
		return 0;
        case 'r':
		cpuValues.setRequested(1);
		return 0;
	case 'd':
		psfreq::Log::setDebug();
		return 0;
	case 'a':
		psfreq::Log::setAllQuiet();
		return 0;
	case 'q':
		psfreq::Log::setQuiet();
		return 0;
        case 'V':
		printGPL();
		printVersion();
                return -1;
        case 'S':
		cpuValues.setAction(1);
                return 0;
        case 'G':
		cpuValues.setAction(0);
                return 0;
        case 'p':
		if (!cpuValues.setPlan(planFromOptArg(optarg))) {
			if (!psfreq::Log::isAllQuiet()) {
				std::cerr << psfreq::Color::boldRed()
					<< "Failed to set a power plan."
					<< psfreq::Color::reset() << std::endl;
			}
			return 1;
		}
                return 0;
        case 'm':
		cpuValues.setMax(psfreq::stringToNumber(optarg));
                return 0;
	case 'g':
		if (!cpuValues.setGovernor(governorFromOptArg(optarg,
					cpu.getAvailableGovernors()))) {
			if (!psfreq::Log::isAllQuiet()) {
				std::cerr << psfreq::Color::boldRed()
					<< "Failed to set governor."
					<< psfreq::Color::reset() << std::endl;
			}
			return 1;
		}
		return 0;
        case 'n':
		cpuValues.setMin(psfreq::stringToNumber(optarg));
		return 0;
        case 't':
		cpuValues.setTurbo(psfreq::stringToNumber(optarg));
		return 0;
        case '1':
		psfreq::Color::setEnabled();
		return 0;
	case ':':
		if (!psfreq::Log::isAllQuiet()) {
			std::cerr << psfreq::Color::boldRed()
				<< "Missing argument for option. "
				<< psfreq::Color::reset() << std::endl;
		}
		return 1;
	case '?':
		if (!psfreq::Log::isAllQuiet()) {
			std::cerr << psfreq::Color::boldRed()
				<< "Unknown option."
				<< psfreq::Color::reset() << std::endl;
		}
		return 1;
	}
	return 1;
}

/*
 * The main program function.
 */
int main(int argc, char** argv)
{
	/*
	 * The cpu and cpuValues are defined here, though by default they
	 * do not actually have the functionality to modify or access much.
	 * The cpu must be initialized at a later period after option parsing
	 * by calling cpu.init()
	 */
	psfreq::Cpu cpu;
	psfreq::Values cpuValues = psfreq::Values(cpu);

	const char *const shortOptions = ":SGHVcrdaqp:m:n:t:g:";
	const struct option longOptions[] = {
                {"help",          no_argument,        NULL,           'H'},
                {"version",       no_argument,        NULL,           'V'},
                {"quiet",         no_argument,        NULL,           'q'},
                {"all-quiet",     no_argument,        NULL,           'a'},
                {"debug",         no_argument,        NULL,           'd'},
                {"get",           no_argument,        NULL,           'G'},
                {"set",           no_argument,        NULL,           'S'},
                {"current",       no_argument,        NULL,           'c'},
                {"real",          no_argument,        NULL,           'r'},
		{"color",	  no_argument,	      NULL,           '1'},
                {"plan",          required_argument,  NULL,           'p'},
                {"governor",      required_argument,  NULL,           'g'},
		{"max",		  required_argument,  NULL,           'm'},
		{"min",		  required_argument,  NULL,           'n'},
		{"turbo",	  required_argument,  NULL,           't'},
		{0,		  0,                  0,              0}
                };

	const int parseResult = parseOptions(argc, argv, cpu, cpuValues,
			shortOptions, longOptions);
	if (parseResult != 0) {
		if (parseResult == -1) {
			return 0;
		} else {
			return 1;
		}
	}

	/*
	 * Initialize the cpu so that it may now act on sysfs values.
	 */
	cpu.init();
	if (!cpuValues.runPlan()) {
		return 1;
	}
	if (cpuValues.isActionNull()) {
		printGPL();
		printHelp();
		return 0;
	} else if (cpuValues.isActionGet()) {
		if (cpuValues.getRequested() == 0) {
			printCpuValues(cpu);
		} else {
			printRealtimeFrequency(cpu);
		}
	} else {
		/*
		 * User must have root privilages to set
		 * values here.
		 */
		if (geteuid() == 0) {
			if (!cpuValues.isInitialized()) {
				if (!psfreq::Log::isAllQuiet()) {
					std::cerr << psfreq::Color::boldRed()
						<< "No Requests."
						<< psfreq::Color::reset()
						<< std::endl;
				}
				return 1;
			}
			if (!setCpuValues(cpu, cpuValues)) {
				if (!psfreq::Log::isAllQuiet()) {
					std::cerr
						<< psfreq::Color::boldRed()
						<< "Environment was"
						<< " not sane. Could"
						<< " not set any"
						<< " values"
						<< psfreq::Color::reset()
						<< std::endl;
				}
				return 1;
			}
			printCpuValues(cpu);
		} else {
			if (!psfreq::Log::isAllQuiet()) {
				std::cerr << psfreq::Color::boldRed()
					<< "Permissions Error."
					<< psfreq::Color::reset() << std::endl;
			}
			return 1;
		}
	}
	return 0;
}

/*
 * As long as command line options exist, loop over the input and
 * run the getopt_long function to figure out the option requested.
 */
int parseOptions(const int argc, char **const argv,
		const psfreq::Cpu &cpu, psfreq::Values &cpuValues,
		const char *const shortOptions,
		const struct option longOptions[]) {
	int finalOptionResult = 0;
	int optionResult = 0;
	while (true) {
                optionResult = getopt_long(argc, argv, shortOptions,
				longOptions, NULL);
                if (optionResult == -1) {
                        break;
                } else {
			finalOptionResult = handleOptionResult(cpu, cpuValues,
					optionResult);
			if (finalOptionResult == -1) {
				return -1;
			} else if (finalOptionResult == 1) {
				if (!psfreq::Log::isAllQuiet()) {
					std::cerr << psfreq::Color::boldRed()
						<< "Bad Option."
						<< psfreq::Color::reset()
						<< std::endl;
				}
				return 1;
			}
                }
        }
	return 0;
}
