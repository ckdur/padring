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

#ifndef csvwriter_h
#define csvwriter_h

#include <stdio.h>
#include <stdint.h>
#include <complex>
#include <string>
#include <sstream>

#include "layout.h"
#include "padringdb.h"

/** a CSV writer which describes positions of pads */
class CSVWriter
{
public:
    CSVWriter(std::ostream &os);
    virtual ~CSVWriter();

    void writePadring(PadringDB *padring);
    void writeCell(const LayoutItem *item, Layout::side_t s);

protected:

    void writeToFile();

    std::stringstream   m_ss;
    std::ostream        &m_def;
    int                 m_side;
    std::string         m_designName;
};

#endif
