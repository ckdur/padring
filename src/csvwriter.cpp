/*
    PADRING -- a padring generator for ASICs.

    Copyright (c) 2019, Niels Moseley <niels@symbioticeda.com>

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

#include <sstream>
#include <fstream>
#include <iomanip>
#include <complex>
#include <math.h>
#include <assert.h>
#include "logging.h"
#include "csvwriter.h"

CSVWriter::CSVWriter(std::ostream &os)
    : m_def(os), m_side(0)
{
    m_designName = "No Name";
}

CSVWriter::~CSVWriter()
{
    m_def.flush();    
    writeToFile();
}

void CSVWriter::writeToFile()
{
    m_def << "Back to Index,\n";
    m_def << ",Pin Assignment (" << m_designName << "),\n";
    m_def << ",\n";
    m_def << ",,,,,Pin Name,\n";
    m_def << ",List,Pin No.,Pin Assign,Association,I/O name,I/O Cell,-,Bond Name,Bond Cell,x,y,rotation,cx,cy,\n";

    m_def << m_ss.str();
}

void CSVWriter::writeCell(const LayoutItem *item, Layout::side_t s)
{
    if(item->m_ltype != LayoutItem::TYPE_CELL && item->m_ltype != LayoutItem::TYPE_BOND)
        return;
    double rot = 0.0;
    double x = item->m_x;
    double y = item->m_y;

    // regular cells have N,S,E,W,
    // corner cells have NE,NW,SE,SW
    if (item->m_location == "N")
    {
        // North orientation, rotation = 180 degrees
        x += item->m_lefinfo->m_sx;
        rot = 180.0;
    }
    else if (item->m_location == "S")
    {
        // South orientation, rotation = 0 degrees
    }
    else if (item->m_location == "E")
    {
        // East orientation
        rot = 90.0;
    }
    else if (item->m_location == "W")
    {
        // West 
        y += item->m_lefinfo->m_sx;
        rot = 270.0;
    }

    // do corners
    if (item->m_location == "NW")
    {
        // North West orientation, rotation = 270 degrees
        rot = 270.0;
    }
    else if (item->m_location == "SE")
    {
        // South East orientation, rotation = 90 degrees
        x += item->m_lefinfo->m_sy;
        rot = 90.0;
    }
    else if (item->m_location == "NE")
    {
        x += item->m_lefinfo->m_sx;
        rot = 180.0;
    }

    std::complex<double> ll = {0.0,0.0};
    std::complex<double> ul = {0.0,item->m_lefinfo->m_sy};
    std::complex<double> ur = {item->m_lefinfo->m_sx,item->m_lefinfo->m_sy};
    std::complex<double> lr = {item->m_lefinfo->m_sx,0.0};

    std::complex<double> rr = {cos(3.1415927*rot/180.0), sin(3.1415927*rot/180.0)};

    ll *= rr;
    ul *= rr;
    ur *= rr;
    lr *= rr;

    ll += std::complex<double>(x,y);
    ul += std::complex<double>(x,y);
    ur += std::complex<double>(x,y);
    lr += std::complex<double>(x,y);
    double cx = 0.5*(ll.real() + ur.real());
    double cy = 0.5*(ll.imag() + ur.imag());

    if (item->m_ltype == LayoutItem::TYPE_CELL && !item->m_havebond)
    {
        m_side++;
        if(s == Layout::side_t::SIDE_SOUTH)
            m_ss << ",SOUTH,";
        if(s == Layout::side_t::SIDE_NORTH)
            m_ss << ",NORTH,";
        if(s == Layout::side_t::SIDE_EAST)
            m_ss << ",EAST,";
        if(s == Layout::side_t::SIDE_WEST)
            m_ss << ",WEST,";
        // PAD stuff
        m_ss << m_side << ",I/O,NONE," << item->m_instance << ","<< item->m_cellname << ",-,";
        // The bond is the same pad
        m_ss << item->m_instance << ","<< item->m_cellname << ",";
        // Location stuff (using the pad as the bond)
        m_ss << x << "," << y << "," << item->m_location << ",";
        m_ss << cx << "," << cy << ",\n";
    }

    if (item->m_ltype == LayoutItem::TYPE_BOND)
    {
        m_side++;
        if(s == Layout::side_t::SIDE_SOUTH)
            m_ss << ",SOUTH,";
        if(s == Layout::side_t::SIDE_NORTH)
            m_ss << ",NORTH,";
        if(s == Layout::side_t::SIDE_EAST)
            m_ss << ",EAST,";
        if(s == Layout::side_t::SIDE_WEST)
            m_ss << ",WEST,";
        // PAD stuff
        m_ss << m_side << ",I/O,NONE,";
        if(item->m_ref != nullptr)
            m_ss << item->m_ref->m_instance << ","<< item->m_ref->m_cellname << ",-,";
        else
            m_ss << item->m_instance << ","<< item->m_cellname << ",-,";
        // Info about the bond
        m_ss << item->m_instance << ","<< item->m_cellname << ",";
        // Location stuff (using the pad as the bond)
        m_ss << x << "," << y << "," << item->m_location << ",";
        m_ss << cx << "," << cy << ",\n";
    }
}

void CSVWriter::writePadring(PadringDB *padring)
{
    // Enumerate
    for(auto item : padring->m_south)
    {
        writeCell(item, Layout::SIDE_SOUTH);
    }
    for(auto item : padring->m_east)
    {
        writeCell(item, Layout::SIDE_EAST);
    }
    for (auto item = padring->m_north.rbegin(); item != padring->m_north.rend(); ++item)
    {
        writeCell((*item), Layout::SIDE_NORTH);
    }
    for (auto item = padring->m_west.rbegin(); item != padring->m_west.rend(); ++item)
    {
        writeCell((*item), Layout::SIDE_WEST);
    }
    m_designName = padring->m_designName;
}

