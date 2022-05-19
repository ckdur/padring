/*
    PADRING -- a padring generator for ASICs.

    Copyright (c) 2019, Niels Moseley <niels@symbioticeda.com>
    Copyright (c) 2022, Ckristian Duran <duran@vlsilab.ee.uec.ac.jp>

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
    
*/

#include "prlefreader.h"
#include "logging.h"

PRLEFReader::PRLEFReader() : m_parseCell(nullptr)
{
    m_lefDatabaseUnits = 0.0f;
}

void PRLEFReader::onMacro(const std::string &macroName)
{
    // perform integrity checks on the previous cell
    if (m_parseCell != nullptr)
    {
        doIntegrityChecks();
    }

    // note: unordered_map::insert will only insert the element
    // if the key is not already present.
    // Therefore, we must first check if a cell/key is already 
    // present and handle it accordingly.

    auto iter = m_cells.find(macroName);
    if (iter != m_cells.end())
    {
        doLog(LOG_WARN,"Cell %s already in database - replaced\n", macroName.c_str());
        m_parseCell = iter->second;
    }
    else
    {
        m_parseCell = new LEFCellInfo_t();
        m_parseCell->m_name = macroName;
        m_cells.insert(std::make_pair(macroName, m_parseCell));

        doLog(LOG_VERBOSE,"Added LEF cell %s\n", macroName.c_str());
    }
}

PRLEFReader::LEFCellInfo_t *PRLEFReader::getCellByName(const std::string &macroName) const
{
    auto iter = m_cells.find(macroName);
    if (iter == m_cells.end())
    {
        return nullptr;
    }
    return iter->second;
}

void PRLEFReader::onSize(double sx, double sy)
{
    if (m_parseCell == nullptr)
    {
        doLog(LOG_ERROR, "PRLEFReader: got size before finding a macro\n");
        return;
    }

    m_parseCell->m_sx = sx;
    m_parseCell->m_sy = sy;
}

void PRLEFReader::onForeign(const std::string &foreignName, double ox, double oy)
{
    if (m_parseCell == nullptr)
    {
        doLog(LOG_ERROR, "PRLEFReader: got foreign before finding a macro\n");
        return;
    }

    m_parseCell->m_foreign = foreignName;
}

void PRLEFReader::onSymmetry(const std::string &symmetry)
{
    if (m_parseCell == nullptr)
    {
        doLog(LOG_ERROR, "PRLEFReader: got symmetry before finding a macro\n");
        return;
    }

    m_parseCell->m_symmetry = symmetry;
}

void PRLEFReader::doIntegrityChecks()
{
    // perform integrity checks on the current cell
    if (m_parseCell == nullptr)
    {
        doLog(LOG_ERROR, "PRLEFReader: cannot do integrity checks on a NULL cell\n");
        return;
    }

    if (m_parseCell->m_foreign.empty())
    {
        m_parseCell->m_foreign = m_parseCell->m_name;
    }

    if (m_parseCell->m_name.empty())
    {
        doLog(LOG_ERROR, "PRLEFReader: current cell has no name!\n");
        return;
    }

    if ((m_parseCell->m_sx == 0.0) || (m_parseCell->m_sy == 0.0))
    {
        doLog(LOG_ERROR,"PRLEFReader: cell %s has zero width or height\n",
            m_parseCell->m_name.c_str());
    }
}

void PRLEFReader::onClass(const std::string &className)
{
    if (className.find("SPACER") != std::string::npos)
    {
        m_parseCell->m_isFiller = true;
    }
    else
    {
        m_parseCell->m_isFiller = false;
    }
}

void PRLEFReader::onDatabaseUnitsMicrons(double unitsPerMicron)
{
    m_lefDatabaseUnits = unitsPerMicron;
    //doLog(LOG_INFO,"LEF database units: %f units per micron\n", unitsPerMicron);
}

void PRLEFReader::onPin(const std::string &pinName) {
    auto iter = m_parseCell->m_pins.find(pinName);
    if (iter != m_parseCell->m_pins.end())
    {
        doLog(LOG_WARN,"Pin %s already in database - replaced\n", pinName.c_str());
        m_parsePin = iter->second;
    }
    else
    {
        m_parsePin = new LEFPinInfo_t();
        m_parseCell->m_name = pinName;
        m_parseCell->m_pins.insert(std::make_pair(pinName, m_parsePin));

        doLog(LOG_VERBOSE,"Added LEF pin %s\n", pinName.c_str());
    }
}

void PRLEFReader::onPinDirection(const std::string &direction) {
    if (direction.find("INPUT") != std::string::npos){
        m_parsePin->m_dir = 0;
    }
    else if (direction.find("OUTPUT") != std::string::npos){
        m_parsePin->m_dir = 1;
    }
    else{
        m_parsePin->m_dir = 2;
    }
}

void PRLEFReader::onPinUse(const std::string &use) {
    if (use.find("SIGNAL") != std::string::npos){
        m_parsePin->m_use = 0;
    }
    else if (use.find("POWER") != std::string::npos){
        m_parsePin->m_use = 1;
    }
    else if (use.find("GROUND") != std::string::npos){
        m_parsePin->m_use = 2;
    }
    else{
        m_parsePin->m_use = 0;
    }
}

void PRLEFReader::onPinLayerClass(const std::string &className) {
    if (className.find("CORE") != std::string::npos){
        m_parsePin->m_class = 1;
    }
}

