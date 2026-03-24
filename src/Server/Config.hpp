#ifndef _CONFIG_HPP
#define _CONFIG_HPP

#include "../Common/IniFile.hpp"

class Config
{
public:
	Config();

	void Reload();

	String GetListeningAddress();
	String GetListeningPort();

	String GetMessageSizeLimit();

private:
	IniFile _configFile;

	void Init();
};

#endif
