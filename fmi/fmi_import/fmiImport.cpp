/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil;  eval: (c-set-offset 'innamespace 0); -*- */
/*
* LLNS Copyright Start
* Copyright (c) 2016, Lawrence Livermore National Security
* This work was performed under the auspices of the U.S. Department
* of Energy by Lawrence Livermore National Laboratory in part under
* Contract W-7405-Eng-48 and in part under Contract DE-AC52-07NA27344.
* Produced at the Lawrence Livermore National Laboratory.
* All rights reserved.
* For details, see the LICENSE file.
* LLNS Copyright End
*/

#include "fmiImport.h"
#include "fmiObjects.h"
#include "zipUtilities.h"

#include <memory>
#include "basicDefs.h"
#include "stringOps.h"
#include <map>
#include <boost/dll/shared_library.hpp>
#include <boost/dll/import.hpp>



using namespace boost::filesystem;

fmiLibrary::fmiLibrary()
{
	information = std::make_shared<fmiInfo>();
}

fmiLibrary::fmiLibrary(const std::string &fmupath)
{
	information = std::make_shared<fmiInfo>();

	loadFMU(fmupath);
}

fmiLibrary::fmiLibrary(const std::string &fmupath, const std::string &extractPath):extractDirectory(extractPath),fmuName(fmupath)
{

	information = std::make_shared<fmiInfo>();
	if (!exists(extractDirectory))
	{
		create_directories(extractDirectory);
	}
	loadInformation();
}

fmiLibrary::~fmiLibrary()
{
	
}

void fmiLibrary::close()
{
	soMeLoaded = false;
	soCoSimLoaded = false;
	lib = nullptr;
	
}

bool fmiLibrary::checkFlag(fmuCapabilityFlags flag) const
{
	return information->checkFlag(flag);
}

bool fmiLibrary::isSoLoaded(fmutype_t type) const
{
	switch(type)
	{
	case fmutype_t::modelExchange:
		return soMeLoaded;
	case fmutype_t::cosimulation:
		return soCoSimLoaded;
	default:
		return (soMeLoaded || soCoSimLoaded);
	}
}

void fmiLibrary::loadFMU(const std::string &fmupath)
{
	path ipath(fmupath);
	if (is_directory(ipath))
	{
		extractDirectory = ipath;
	}
	else
	{
		fmuName = ipath;
		extractDirectory = fmuName.parent_path() / fmuName.stem();
	}
	loadInformation();
}



void fmiLibrary::loadFMU(const std::string &fmupath, const std::string &extractLoc)
{
	extractDirectory = extractLoc;
	fmuName = fmupath;
	loadInformation();
}

int fmiLibrary::getCounts(const std::string &countType) const
{
	size_t cnt = size_t(-1);
	if (countType=="meobjects")
	{
		cnt = mecount;
	}
	else if (countType == "cosimobjects")
	{
		cnt = cosimcount;
	}
	else 
	{
		cnt = information->getCounts(countType);
	}
	
	if (cnt== size_t(-1))
	{
		return (-1);
	}
	return static_cast<int>(cnt);
}

void fmiLibrary::loadInformation()
{
	auto xmlfile = extractDirectory / "modelDescription.xml";
	if (!exists(xmlfile))
	{
		auto res = extract();
		if (res!=0)
		{
			return;
		}
	}
	int res = information->loadFile(xmlfile.string());
	if (res!=0)
	{
		error = true;
		return;
	}
	xmlLoaded = true;

	//load the resources directory location if it exists
	if (exists(extractDirectory / "resources"))
	{
		resourceDir = extractDirectory / "resources";
	}
	else
	{
		resourceDir = "";
	}
}

int fmiLibrary::extract()
{
	int ret = unzip(fmuName.string(), extractDirectory.string());
	if (ret != 0)
	{
		error = true;
	}
	return ret;
}






std::shared_ptr<fmi2ME> fmiLibrary::createModelExchangeObject(const std::string &name)
{
	if (soMeLoaded)
	{
		if (!callbacks)
		{
			makeCallbackFunctions();
		}
		auto comp = baseFunctions.fmi2Instantiate(name.c_str(), fmi2ModelExchange, information->getString("guid").c_str(), resourceDir.string().c_str(), reinterpret_cast<fmi2CallbackFunctions *>(callbacks.get()), fmi2False, fmi2False);
		auto meobj = std::make_shared<fmi2ME>(comp,information,commonFunctions,MEFunctions);
		++mecount;
		return meobj;
	}

	return nullptr;
}

std::shared_ptr<fmi2CoSim> fmiLibrary::createCoSimulationObject(const std::string &name)
{
	if (soCoSimLoaded)
	{
		if (!callbacks)
		{
			makeCallbackFunctions();
		}
		auto comp = baseFunctions.fmi2Instantiate(name.c_str(), fmi2CoSimulation, information->getString("guid").c_str(), resourceDir.string().c_str(), reinterpret_cast<fmi2CallbackFunctions *>(callbacks.get()), fmi2False, fmi2False);
		auto csobj = std::make_shared<fmi2CoSim>(comp,information, commonFunctions, CoSimFunctions);
		++cosimcount;
		return csobj;
	}
	return nullptr;
}




void fmiLibrary::loadSharedLibrary(fmutype_t type)
{
	//TODO:: make a check for already loaded
	auto sopath = findSoPath(type);
	bool loaded = false;
	if (!sopath.empty())
	{
		lib = std::make_shared<boost::dll::shared_library>(sopath);
		if (lib->is_loaded())
		{
			loaded = true;
		}
	}
	if (loaded)
	{
		loadBaseFunctions();
		loadCommonFunctions();
		//Only load one or the other
		if (checkFlag(modelExchangeCapable) && type!=fmutype_t::cosimulation)
		{
			loadMEFunctions();
			soMeLoaded = true;
			soCoSimLoaded = false;
		}
		else if (checkFlag(coSimulationCapable))
		{
			loadCoSimFunctions();
			soCoSimLoaded = true;
			soMeLoaded = false;
		}
	}
	

}


path fmiLibrary::findSoPath(fmutype_t type)
{
	path sopath = extractDirectory / "binaries";

	std::string identifier;
	switch(type)
	{
	case fmutype_t::unknown:
	default:
		if (checkFlag(modelExchangeCapable)) //give priority to model Exchange
		{
			identifier = information->getString("meidentifier");
		}
		else if (checkFlag(coSimulationCapable))
		{
			identifier = information->getString("cosimidentifier");
		}
		else
		{
			return "";
		}
		break;
	case fmutype_t::cosimulation:
		if (checkFlag(coSimulationCapable))
		{
			identifier = information->getString("cosimidentifier");
		}
		else
		{
			return "";
		}
	case fmutype_t::modelExchange:
		if (checkFlag(modelExchangeCapable))
		{
			identifier = information->getString("meidentifier");
		}
		else
		{
			return "";
		}
	}
#ifdef _WIN32
	sopath /= "win64";
	sopath/=identifier + ".dll";
#else 
#ifdef MACOS
        sopath /= "darwin64";
	sopath/= identifier + ".so";
#else
	sopath /= "linux64";
	sopath/= identifier + ".so";
#endif
#endif

	if (exists(sopath))
	{
		return sopath;
	}
	//try the alternate paths
	sopath = extractDirectory / "binaries";
#ifdef _WIN32
	sopath /= "win32";
	sopath /=identifier + ".dll";
#else 
#ifdef MACOS
        sopath /= "darwin32";
	sopath /=identifier + ".so";
#else
	sopath /= "linux32";
	sopath /= identifier + ".so";
#endif
#endif
	if (exists(sopath))
	{
		return sopath;
	}

	return path("");
}


void fmiLibrary::loadBaseFunctions()
{
	baseFunctions.fmi2GetVersion = lib->get<fmi2GetVersionTYPE>("fmi2GetVersion");
	baseFunctions.fmi2GetTypesPlatform = lib->get<fmi2GetTypesPlatformTYPE>("fmi2GetTypesPlatform");
	baseFunctions.fmi2Instantiate = lib->get<fmi2InstantiateTYPE>("fmi2Instantiate");	
}

//TODO:: PT move these to the constructors of the function libraries
void fmiLibrary::loadCommonFunctions()
{
	commonFunctions = std::make_shared<fmiCommonFunctions>();

	commonFunctions->lib = lib;
	commonFunctions->fmi2SetDebugLogging = lib->get<fmi2SetDebugLoggingTYPE>("fmi2SetDebugLogging");

	/* Creation and destruction of FMU instances and setting debug status */

	commonFunctions->fmi2FreeInstance = lib->get<fmi2FreeInstanceTYPE>("fmi2FreeInstance");

	/* Enter and exit initialization mode, terminate and reset */
	commonFunctions->fmi2SetupExperiment = lib->get<fmi2SetupExperimentTYPE>("fmi2SetupExperiment");
	commonFunctions->fmi2EnterInitializationMode = lib->get<fmi2EnterInitializationModeTYPE>("fmi2EnterInitializationMode");
	commonFunctions->fmi2ExitInitializationMode = lib->get<fmi2ExitInitializationModeTYPE>("fmi2ExitInitializationMode");
	commonFunctions->fmi2Terminate = lib->get<fmi2TerminateTYPE>("fmi2Terminate");
	commonFunctions->fmi2Reset = lib->get<fmi2ResetTYPE>("fmi2Reset");

	/* Getting and setting variable values */
	commonFunctions->fmi2GetReal = lib->get<fmi2GetRealTYPE>("fmi2GetReal");
	commonFunctions->fmi2GetInteger = lib->get<fmi2GetIntegerTYPE>("fmi2GetInteger");
	commonFunctions->fmi2GetBoolean = lib->get<fmi2GetBooleanTYPE>("fmi2GetBoolean");
	commonFunctions->fmi2GetString = lib->get<fmi2GetStringTYPE>("fmi2GetString");

	commonFunctions->fmi2SetReal = lib->get<fmi2SetRealTYPE>("fmi2SetReal");
	commonFunctions->fmi2SetInteger = lib->get<fmi2SetIntegerTYPE>("fmi2SetInteger");
	commonFunctions->fmi2SetBoolean = lib->get<fmi2SetBooleanTYPE>("fmi2SetBoolean");
	commonFunctions->fmi2SetString = lib->get<fmi2SetStringTYPE>("fmi2SetString");

	/* Getting and setting the internal FMU state */
	commonFunctions->fmi2GetFMUstate = lib->get<fmi2GetFMUstateTYPE>("fmi2GetFMUstate");
	commonFunctions->fmi2SetFMUstate = lib->get<fmi2SetFMUstateTYPE>("fmi2SetFMUstate");
	commonFunctions->fmi2FreeFMUstate = lib->get<fmi2FreeFMUstateTYPE>("fmi2FreeFMUstate");
	commonFunctions->fmi2SerializedFMUstateSize = lib->get<fmi2SerializedFMUstateSizeTYPE>("fmi2SerializedFMUstateSize");
	commonFunctions->fmi2SerializeFMUstate = lib->get<fmi2SerializeFMUstateTYPE>("fmi2SerializeFMUstate");
	commonFunctions->fmi2DeSerializeFMUstate = lib->get<fmi2DeSerializeFMUstateTYPE>("fmi2DeSerializeFMUstate");

	/* Getting partial derivatives */
	commonFunctions->fmi2GetDirectionalDerivative = lib->get<fmi2GetDirectionalDerivativeTYPE>("fmi2GetDirectionalDerivative");
}
void fmiLibrary::loadMEFunctions()
{
	MEFunctions = std::make_shared<fmiMEFunctions>();

	MEFunctions->lib = lib;
	MEFunctions->fmi2EnterEventMode = lib->get<fmi2EnterEventModeTYPE>("fmi2EnterEventMode");
	MEFunctions->fmi2NewDiscreteStates = lib->get<fmi2NewDiscreteStatesTYPE>("fmi2NewDiscreteStates");
	MEFunctions->fmi2EnterContinuousTimeMode = lib->get<fmi2EnterContinuousTimeModeTYPE>("fmi2EnterContinuousTimeMode");
	MEFunctions->fmi2CompletedIntegratorStep = lib->get<fmi2CompletedIntegratorStepTYPE>("fmi2CompletedIntegratorStep");

	/* Providing independent variables and re-initialization of caching */
	MEFunctions->fmi2SetTime = lib->get<fmi2SetTimeTYPE>("fmi2SetTime");
	MEFunctions->fmi2SetContinuousStates = lib->get<fmi2SetContinuousStatesTYPE>("fmi2SetContinuousStates");

	/* Evaluation of the model equations */
	MEFunctions->fmi2GetDerivatives = lib->get<fmi2GetDerivativesTYPE>("fmi2GetDerivatives");
	MEFunctions->fmi2GetEventIndicators = lib->get<fmi2GetEventIndicatorsTYPE>("fmi2GetEventIndicators");
	MEFunctions->fmi2GetContinuousStates = lib->get<fmi2GetContinuousStatesTYPE>("fmi2GetContinuousStates");
	MEFunctions->fmi2GetNominalsOfContinuousStates = lib->get<fmi2GetNominalsOfContinuousStatesTYPE>("fmi2GetNominalsOfContinuousStates");
}
void fmiLibrary::loadCoSimFunctions()
{
	CoSimFunctions = std::make_shared<fmiCoSimFunctions>();
	CoSimFunctions->lib = lib;
	CoSimFunctions->fmi2SetRealInputDerivatives = lib->get<fmi2SetRealInputDerivativesTYPE>("fmi2SetRealInputDerivatives");
	CoSimFunctions->fmi2GetRealOutputDerivatives = lib->get<fmi2GetRealOutputDerivativesTYPE>("fmi2GetRealOutputDerivatives");

	CoSimFunctions->fmi2DoStep = lib->get<fmi2DoStepTYPE>("fmi2DoStep");
	CoSimFunctions->fmi2CancelStep = lib->get<fmi2CancelStepTYPE>("fmi2CancelStep");

	/* Inquire slave status */
	CoSimFunctions->fmi2GetStatus = lib->get<fmi2GetStatusTYPE>("fmi2GetStatus");
	CoSimFunctions->fmi2GetRealStatus = lib->get<fmi2GetRealStatusTYPE>("fmi2GetRealStatus");
	CoSimFunctions->fmi2GetIntegerStatus = lib->get<fmi2GetIntegerStatusTYPE>("fmi2GetIntegerStatus");
	CoSimFunctions->fmi2GetBooleanStatus = lib->get<fmi2GetBooleanStatusTYPE>("fmi2GetBooleanStatus");
	CoSimFunctions->fmi2GetStringStatus = lib->get<fmi2GetStringStatusTYPE>("fmi2GetStringStatus");
}


void fmiLibrary::makeCallbackFunctions()
{
	callbacks = std::make_shared<fmi2CallbackFunctions_nc>();
	callbacks->allocateMemory = &calloc;
	callbacks->freeMemory = &free;
	callbacks->logger = &loggerFunc;
	callbacks->componentEnvironment = (void *)(this);

}


void loggerFunc(fmi2ComponentEnvironment, fmi2String, fmi2Status, fmi2String, fmi2String, ...)
{
	
}
