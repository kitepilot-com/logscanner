/*******************************************************************************
 * File: logscanner/LumpContainer.h                                            *
 * Created:     Apr-14-25                                                      *
 * Last update: Apr-14-25                                                      *
 * Top level header file.                                                      *
 *                                                                             *
 * This class implements the LUMP logfile type.                                *
 *******************************************************************************/

#ifndef LUMP_CONTAINER_H
#define LUMP_CONTAINER_H

#include <set>
#include <string>

#include "ConfContainer.h"

class LumpContainer : public ConfContainer
{
	public:
		static constexpr char m_LUMP[] = "LUMP";
		static constexpr int  m_SIZE   = strlen(m_LUMP);

		LumpContainer(std::string logFilePath, std::map<char, std::string> extrctDate, std::map<std::string, char> regexpList);
		virtual ~LumpContainer();

//		virtual bool        allIsGood();
		virtual std::string getLumpComponent();

	protected:
	private:
		std::set<std::string>           m_logList;
		std::set<std::string>::iterator m_logListIter;
};

#endif // LUMP_CONTAINER_H
/*END*/
