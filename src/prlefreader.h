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

#ifndef prlefreader_h
#define prlefreader_h

#include <string>
#include <unordered_map>

#include "lef/lefreader.h"

/** LEF Reader + cell database */
class PRLEFReader : public LEFReader
{
public:
    PRLEFReader();

    /** callback for each LEF macro */
    virtual void onMacro(const std::string &macroName) override;

    /** callback for CLASS within a macro */
    virtual void onClass(const std::string &className) override;

    /** callback for FOREIGN within a macro */
    virtual void onForeign(const std::string &foreignName, double x, double y) override;

    /** callback for SIZE within a macro */
    virtual void onSize(double sx, double sy) override;

    /** callback for SYMMETRY within a macro */
    virtual void onSymmetry(const uint32_t symmetry) override;


    /** callback for UNITS DATABASE MICRONS */
    virtual void onDatabaseUnitsMicrons(double unitsPerMicron) override;

    /** callback for PIN within a macro */
    virtual void onPin(const std::string &pinName) override;

    /** callback for PIN direction */
    virtual void onPinDirection(const std::string &direction) override;

    /** callback for PIN use */
    virtual void onPinUse(const std::string &use) override;

    /** callback for PIN PORT CLASS use */
    virtual void onPinLayerClass(const std::string &className) override;

    void doIntegrityChecks();
    
    class LEFPinInfo_t
    {
    public:
        LEFPinInfo_t() : m_dir(0), m_class(0), m_use(0) {}

        int             m_dir;      ///< direction (0: in, 1: out, 2:inout)
        int             m_class;    ///< class (0: none, 1: core)
        int             m_use;      ///< usage (0: signal, 1: power, 2: ground)
    };

    class LEFCellInfo_t
    {
    public:
        LEFCellInfo_t() : m_sx(0.0), m_sy(0.0) {}

        std::string     m_name;     ///< LEF cell name
        std::string     m_foreign;  ///< foreign name
        double          m_sx;       ///< size in microns
        double          m_sy;       ///< size in microns
        uint32_t        m_symmetry; ///< symmetry string taken from LEF.
        bool            m_isFiller; ///< whenever this cell is a filler.
        std::unordered_map<std::string, LEFPinInfo_t*> m_pins;       
    };

    LEFCellInfo_t *getCellByName(const std::string &name) const;
    LEFCellInfo_t *m_parseCell;   ///< current cell being parsed
    LEFPinInfo_t  *m_parsePin;   ///< current pin being parsed
    
    std::unordered_map<std::string, LEFCellInfo_t*> m_cells;

    double m_lefDatabaseUnits;      ///< database units in microns
};

#endif
