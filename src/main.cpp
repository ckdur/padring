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
    
    Changelog:

    0.02b - first release.
    0.02c - exit when space can't be filled by filler cells.

*/

#include <iostream>
#include <fstream>

#define __PGMVERSION__ "0.02c"

#include "logging.h"

#include "cxxopts.h"
#include "prlefreader.h"
#include "configreader.h"
#include "layout.h"
#include "padringdb.h"
#include "svgwriter.h"
#include "defwriter.h"
#include "verilogwriter.h"
#include "csvwriter.h"
#include "fillerhandler.h"
#include "debugutils.h"
#include "gds2/gds2writer.h"

int main(int argc, char *argv[])
{
    setLogLevel(LOG_INFO);

    cxxopts::Options options("padring","PADRING - Symbiotic EDA GmbH\ngenerates a GDS2 file containing a padring");

    options
        .positional_help("config_file")
        .show_positional_help();

    options.add_options()
        ("h,help", "Print help")
        ("L,lef", "LEF file", cxxopts::value<std::vector<std::string>>())
        ("o,output", "GDS2 output file", cxxopts::value<std::string>())
        ("svg", "SVG output file", cxxopts::value<std::string>())
        ("def", "DEF output file", cxxopts::value<std::string>())
        ("ver", "Verilog output file", cxxopts::value<std::string>())
        ("csv", "CSV output file", cxxopts::value<std::string>())
        ("q,quiet", "produce no console output")
        ("v,verbose", "produce verbose output")
        ("filler", "set the filler cell prefix", cxxopts::value<std::vector<std::string>>())
        ("positional",
            "", cxxopts::value<std::vector<std::string>>());

    options.parse_positional({"configfile", "positional"});

    auto cmdresult = options.parse(argc, argv);

    if ((cmdresult.count("help")>0) || 
        (cmdresult.count("positional")!=1))
    {
        std::cout << options.help({"", "Group"}) << std::endl;
        exit(0);
    }

    /** set log level according to command line options */
    if (cmdresult.count("quiet") > 0)
    {
        setLogLevel(LOG_QUIET);
    }
    else if (cmdresult.count("verbose") > 0)
    {
        setLogLevel(LOG_VERBOSE);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Program banner
    ////////////////////////////////////////////////////////////////////////////////
    doLog(LOG_INFO,"PADRING version " __PGMVERSION__ " - compiled on " __DATE__ "\n");
    doLog(LOG_INFO,"Symbiotic EDA GmbH\n\n");

    if (cmdresult.count("lef") < 1)
    {
        std::cout << "You must specify at least one LEF file containing the ASIC cells";
        exit(0);
    }

    PadringDB padring;

    double LEFDatabaseUnits = 0.0;

    // read the cells from the LEF files
    // and save the most recent database units figure along the way..
    auto &leffiles = cmdresult["lef"].as<std::vector<std::string> >();
    for(auto leffile : leffiles)
    {
        std::ifstream lefstream(leffile, std::ifstream::in);
        doLog(LOG_INFO, "Reading LEF %s\n", leffile.c_str());
        padring.m_lefreader.parse(lefstream);
        if (padring.m_lefreader.m_lefDatabaseUnits > 0.0)
        {
            LEFDatabaseUnits = padring.m_lefreader.m_lefDatabaseUnits;
        }
    }

    doLog(LOG_INFO,"%d cells read\n", padring.m_lefreader.m_cells.size());

    auto& v = cmdresult["positional"].as<std::vector<std::string> >();
    std::string configFileName = v[0];

    std::ifstream configStream(configFileName, std::ifstream::in);
    if (!padring.parse(configStream))
    {
        doLog(LOG_ERROR,"Cannot parse configuration file -- aborting\n");
        exit(1);
    }

    // if an explicit filler cell prefix was not given,
    // search the cell database for filler cells
    FillerHandler fillerHandler;
    fillerHandler.addFillers(&padring.m_lefreader, padring.m_fillers);

    doLog(LOG_INFO, "Found %d filler cells\n", fillerHandler.getCellCount());

    if (fillerHandler.getCellCount() == 0)
    {
        doLog(LOG_ERROR, "Cannot proceed without filler cells. Please use the --filler option to explicitly specify a filler cell prefix\n");
        exit(1);
    }

    // check die size
    if ((padring.m_dieWidth < 1.0e-6) || (padring.m_dieHeight < 1.0e-6))
    {
        doLog(LOG_ERROR, "Die area was not specified! - aborting.\n");
        exit(1);
    }

    // generate report
    doLog(LOG_INFO,"Die area        : %f x %f microns\n", padring.m_dieWidth, padring.m_dieHeight);
    doLog(LOG_INFO,"Grid            : %f microns\n", padring.m_grid);
    doLog(LOG_INFO,"Padring cells   : %d\n", padring.getPadCellCount());
    doLog(LOG_INFO,"Smallest filler : %f microns\n", fillerHandler.getSmallestWidth());
    
    padring.doLayout();

    // get corners
    LayoutItem *topleft  = padring.m_north.getFirstCorner();
    LayoutItem *topright = padring.m_north.getLastCorner();
    LayoutItem *bottomleft  = padring.m_south.getFirstCorner();
    LayoutItem *bottomright = padring.m_south.getLastCorner();

    // write the padring to an SVG file
    std::ofstream svgos;
    if (cmdresult.count("svg") != 0)
    {
        doLog(LOG_INFO,"Writing padring to SVG file: %s\n", cmdresult["svg"].as<std::string>().c_str());
        svgos.open(cmdresult["svg"].as<std::string>(), std::ofstream::out);
        if (!svgos.is_open())
        {
            doLog(LOG_ERROR, "Cannot open SVG file for writing!\n");
            exit(1);
        }
    }

    // write the padring to an DEF file
    std::ofstream defos;
    if (cmdresult.count("def") != 0)
    {
        doLog(LOG_INFO,"Writing padring to DEF file: %s\n", cmdresult["def"].as<std::string>().c_str());
        defos.open(cmdresult["def"].as<std::string>(), std::ofstream::out);
        if (!defos.is_open())
        {
            doLog(LOG_ERROR, "Cannot open DEF file for writing!\n");
            exit(1);
        }
    }

    // write the padring to an Verilog file
    std::ofstream veros;
    if (cmdresult.count("ver") != 0)
    {
        doLog(LOG_INFO,"Writing padring to verilog file: %s\n", cmdresult["ver"].as<std::string>().c_str());
        veros.open(cmdresult["ver"].as<std::string>(), std::ofstream::out);
        if (!veros.is_open())
        {
            doLog(LOG_ERROR, "Cannot open verilog file for writing!\n");
            exit(1);
        }
    }

    // write the padring to an Verilog file
    std::ofstream csvos;
    if (cmdresult.count("csv") != 0)
    {
        doLog(LOG_INFO,"Writing padring to csv file: %s\n", cmdresult["csv"].as<std::string>().c_str());
        csvos.open(cmdresult["csv"].as<std::string>(), std::ofstream::out);
        if (!csvos.is_open())
        {
            doLog(LOG_ERROR, "Cannot open csv file for writing!\n");
            exit(1);
        }
    }

    SVGWriter svg(svgos, padring.m_dieWidth, padring.m_dieHeight);
    DEFWriter def(defos, padring.m_dieWidth, padring.m_dieHeight);
    def.setDatabaseUnits(LEFDatabaseUnits);
    def.setDesignName(padring.m_designName);
    VerilogWriter ver(veros);
    ver.setDesignName(padring.m_designName);
    
    CSVWriter csv(csvos);
    csv.writePadring(&padring);

    // emit GDS2 and SVG
    GDS2Writer *writer = nullptr;
    
    if (cmdresult.count("output")> 0)
    {
        doLog(LOG_INFO,"Writing padring to GDS2 file: %s\n", cmdresult["output"].as<std::string>().c_str());
        writer = GDS2Writer::open(cmdresult["output"].as<std::string>(),
            padring.m_designName);
    }
    
    if (writer != nullptr) writer->writeCell(topleft);
    if (writer != nullptr) writer->writeCell(topright);
    if (writer != nullptr) writer->writeCell(bottomleft);
    if (writer != nullptr) writer->writeCell(bottomright);

    svg.writeCell(topleft);
    svg.writeCell(topright);
    svg.writeCell(bottomleft);
    svg.writeCell(bottomright);

    def.writeCell(topleft);
    def.writeCell(topright);
    def.writeCell(bottomleft);
    def.writeCell(bottomright);

    ver.writeCell(topleft);
    ver.writeCell(topright);
    ver.writeCell(bottomleft);
    ver.writeCell(bottomright);
    
    int numfill = 0;

    double north_y = padring.m_dieHeight;
    for(auto item : padring.m_north)
    {
        if (item->m_ltype == LayoutItem::TYPE_CELL)
        {
            if (writer != nullptr) writer->writeCell(item);
            svg.writeCell(item);
            def.writeCell(item);
            ver.writeCell(item);
        }
        else if (item->m_ltype == LayoutItem::TYPE_BOND)
        {
            if (writer != nullptr) writer->writeCell(item);
            svg.writeCell(item);
            def.writeCell(item);
            ver.writeCell(item);
        }
        else if (item->m_ltype == LayoutItem::TYPE_FILLERDECL) {
            // Re-load the fillers
            fillerHandler.addFillers(&padring.m_lefreader, item->m_fillers);
        }
        else if ((item->m_ltype == LayoutItem::TYPE_FIXEDSPACE) || (item->m_ltype == LayoutItem::TYPE_FLEXSPACE))
        {
            // do fillers
            double space = item->m_size;
            double pos = item->m_x;
            while(space > 0.0)
            {
                std::string cellName;
                double width = fillerHandler.getFillerCell(space, cellName);
                if (width > 0.0)
                {
                    LayoutItem filler(LayoutItem::TYPE_FILLER);
                    filler.m_cellname = cellName;
                    filler.m_instance = "FILLER_" + std::to_string(numfill);
                    filler.m_x = pos;
                    filler.m_y = north_y;
                    filler.m_size = width;
                    filler.m_location = "N";
                    filler.m_lefinfo = padring.m_lefreader.getCellByName(cellName);
                    if (writer != nullptr) writer->writeCell(&filler);
                    svg.writeCell(&filler);
                    def.writeCell(&filler);
                    ver.writeCell(&filler);
                    space -= width;
                    if(space > 0.0 && round(space/padring.m_grid) == 0) {
                      space = 0; // To avoid imprecision
                    }
                    pos += width;
                    numfill++;
                }
                else
                {
                    doLog(LOG_ERROR, "(north) Cannot find filler cell that fits remaining width %g (%d)\n", space, item->m_ltype);
                    exit(1);
                }
            }
        }        
    }

    double south_y = 0;
    for(auto item : padring.m_south)
    {
        if (item->m_ltype == LayoutItem::TYPE_CELL)
        {
            if (writer != nullptr) writer->writeCell(item);
            svg.writeCell(item);  
            def.writeCell(item);            
            ver.writeCell(item);
        }
        else if (item->m_ltype == LayoutItem::TYPE_BOND)
        {
            if (writer != nullptr) writer->writeCell(item);
            svg.writeCell(item);
            def.writeCell(item);
            ver.writeCell(item);
        }
        else if (item->m_ltype == LayoutItem::TYPE_FILLERDECL) {
            // Re-load the fillers
            fillerHandler.addFillers(&padring.m_lefreader, item->m_fillers);
        }
        else if ((item->m_ltype == LayoutItem::TYPE_FIXEDSPACE) || (item->m_ltype == LayoutItem::TYPE_FLEXSPACE))
        {
            // do fillers
            double space = item->m_size;
            double pos = item->m_x;
            while(space > 0.0)
            {
                std::string cellName;
                double width = fillerHandler.getFillerCell(space, cellName);
                if (width > 0.0)
                {
                    LayoutItem filler(LayoutItem::TYPE_FILLER);
                    filler.m_cellname = cellName;
                    filler.m_instance = "FILLER_" + std::to_string(numfill);
                    filler.m_x = pos;
                    filler.m_y = south_y;
                    filler.m_size = width;
                    filler.m_location = "S";
                    filler.m_lefinfo = padring.m_lefreader.getCellByName(cellName);
                    if (writer != nullptr) writer->writeCell(&filler);
                    svg.writeCell(&filler);
                    def.writeCell(&filler);
                    ver.writeCell(&filler);
                    space -= width;
                    if(space > 0.0 && round(space/padring.m_grid) == 0) {
                      space = 0; // To avoid imprecision
                    }
                    pos += width;
                    numfill++;
                }
                else
                {
                    doLog(LOG_ERROR, "(south) Cannot find filler cell that fits remaining width %g (%d)\n", space, item->m_ltype);
                    exit(1);
                }
            }
        }        
    }

    double west_x = 0;
    for(auto item : padring.m_west)
    {
        if (item->m_ltype == LayoutItem::TYPE_CELL)
        {
            if (writer != nullptr) writer->writeCell(item);
            svg.writeCell(item);
            def.writeCell(item);
            ver.writeCell(item);
        }
        else if (item->m_ltype == LayoutItem::TYPE_BOND)
        {
            if (writer != nullptr) writer->writeCell(item);
            svg.writeCell(item);
            def.writeCell(item);
            ver.writeCell(item);
        }
        else if (item->m_ltype == LayoutItem::TYPE_FILLERDECL) {
            // Re-load the fillers
            fillerHandler.addFillers(&padring.m_lefreader, item->m_fillers);
        }
        else if ((item->m_ltype == LayoutItem::TYPE_FIXEDSPACE) || (item->m_ltype == LayoutItem::TYPE_FLEXSPACE))
        {
            // do fillers
            double space = item->m_size;
            double pos = item->m_y;
            while(space > 0.0)
            {
                std::string cellName;
                double width = fillerHandler.getFillerCell(space, cellName);
                if (width > 0.0)
                {
                    LayoutItem filler(LayoutItem::TYPE_FILLER);
                    filler.m_cellname = cellName;
                    filler.m_instance = "FILLER_" + std::to_string(numfill);
                    filler.m_x = west_x;
                    filler.m_y = pos;
                    filler.m_size = width;
                    filler.m_location = "W";
                    filler.m_lefinfo = padring.m_lefreader.getCellByName(cellName);
                    if (writer != nullptr) writer->writeCell(&filler);
                    svg.writeCell(&filler);
                    def.writeCell(&filler);
                    ver.writeCell(&filler);
                    space -= width;
                    if(space > 0.0 && round(space/padring.m_grid) == 0) {
                      space = 0; // To avoid imprecision
                    }
                    pos += width;
                    numfill++;
                }
                else
                {
                    doLog(LOG_ERROR, "(east) Cannot find filler cell that fits remaining width %g (%d)\n", space, item->m_ltype);
                    exit(1);
                }
            }
        }        
    }

    double east_x = padring.m_dieWidth;
    for(auto item : padring.m_east)
    {
        if (item->m_ltype == LayoutItem::TYPE_CELL)
        {
            if (writer != nullptr) writer->writeCell(item);
            svg.writeCell(item);
            def.writeCell(item);
            ver.writeCell(item);
        }
        else if (item->m_ltype == LayoutItem::TYPE_BOND)
        {
            if (writer != nullptr) writer->writeCell(item);
            svg.writeCell(item);
            def.writeCell(item);
            ver.writeCell(item);
        }
        else if (item->m_ltype == LayoutItem::TYPE_FILLERDECL) {
            // Re-load the fillers
            fillerHandler.addFillers(&padring.m_lefreader, item->m_fillers);
        }
        else if ((item->m_ltype == LayoutItem::TYPE_FIXEDSPACE) || (item->m_ltype == LayoutItem::TYPE_FLEXSPACE))
        {
            // do fillers
            double space = item->m_size;
            double pos = item->m_y;
            while(space > 0.0)
            {
                std::string cellName;
                double width = fillerHandler.getFillerCell(space, cellName);
                if (width > 0.0)
                {
                    LayoutItem filler(LayoutItem::TYPE_FILLER);
                    filler.m_cellname = cellName;
                    filler.m_instance = "FILLER_" + std::to_string(numfill);
                    filler.m_x = east_x;
                    filler.m_y = pos;
                    filler.m_size = width;
                    filler.m_location = "E";
                    filler.m_lefinfo = padring.m_lefreader.getCellByName(cellName);
                    if (writer != nullptr) writer->writeCell(&filler);
                    svg.writeCell(&filler);
                    def.writeCell(&filler);
                    ver.writeCell(&filler);
                    space -= width;
                    if(space > 0.0 && round(space/padring.m_grid) == 0) {
                      space = 0; // To avoid imprecision
                    }
                    pos += width;
                    numfill++;
                }
                else
                {
                    doLog(LOG_ERROR, "(west) Cannot find filler cell that fits remaining width %g (%d)\n", space, item->m_ltype);
                    exit(1);
                }
            }
        }        
    }

    if (writer != nullptr) delete writer;

    for(auto cell : padring.m_lefreader.m_cells)
    {
        DebugUtils::dumpToConsole(cell.second);
    }

    return 0;
}

