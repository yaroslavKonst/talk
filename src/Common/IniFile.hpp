#ifndef _INIFILE_HPP
#define _INIFILE_HPP

#include "MyString.hpp"

class IniFile
{
public:
	IniFile();
	IniFile(String path);
	~IniFile();

	String Get(String section, String key);
	void Set(String section, String key, String value);

	String *EnumerateSections(int &sectionCount);

	void Clear();

	void SetPath(String path);
	String GetPath();
	void Write();

private:
	struct Key
	{
		Key *Next;

		String Name;
		String Value;
	};

	struct Section
	{
		Section *Next;

		String Name;
		Key *Keys;
	};

	Section *_data;

	bool _modified;
	String _path;

	void Load();
	void Save();

	void Free();
};

#endif
