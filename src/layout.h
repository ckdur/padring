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

#ifndef layout_h
#define layout_h

#include "prlefreader.h"

#include <string>
#include <list>

class LayoutItem
{
public:
    enum LayoutItemType
    {
        TYPE_CELL,          ///< layout item is a cell with fixed dimensions.
        TYPE_CORNER,        ///< layout item is a corner with fixed dimensions.
        TYPE_FIXEDSPACE,    ///< layout item is a fixed space, to be filled with filler cells.
        TYPE_FLEXSPACE,     ///< layout item is a unspecified space, to be filled with filler cells.
        TYPE_FILLER,        ///< fixed-width filler cell.
        TYPE_BOND,          ///< A bond pad
        TYPE_FILLERDECL     ///< Filler declaration
    };

    LayoutItem(LayoutItemType ltype) : m_lefinfo(nullptr),
        m_ltype(ltype),
        m_size(-1), m_osize(-1),
        m_x(-1.0), m_y(-1.0),
        m_flipped(false),m_offset(0.0),
        m_ref(NULL),m_havebond(false)
    {        
    }

    virtual ~LayoutItem() {}

    PRLEFReader::LEFCellInfo_t *m_lefinfo;  ///< for CELLs and CORNERs, LEF info.

    std::string m_instance; ///< instance name
    std::string m_cellname; ///< cell name
    std::string m_location; ///< location of cell
    double      m_size;     ///< size of the item (-1 if unknown)
    double      m_osize;    ///< size of the item in the other coordinate (-1 if unknown)
    double      m_offset;   ///< offset of the item (0 by default)
    double      m_x;        ///< x-position of item (-1 if unknown)
    double      m_y;        ///< y-position of item (-1 if unknown)
    bool        m_flipped;  ///< when true, unplaced/unrotated cell is filled along y axis.
    std::list<std::string> m_fillers;
    LayoutItemType m_ltype;
    LayoutItem* m_ref;      ///< Reference for bonds
    bool        m_havebond; ///< For TYPE_CELL, is there a bond?
};



class Layout
{
public:
    enum direction_t
    {
        DIR_HORIZONTAL,
        DIR_VERTICAL
    };
    enum side_t
    {
        SIDE_NORTH,
        SIDE_SOUTH,
        SIDE_EAST,
        SIDE_WEST
    };

    Layout(direction_t dir, side_t side);

    virtual ~Layout();

    /** Set the die size in the layout direction */
    void setDieSize(double dieSize) { m_dieSize = dieSize; }

    /** Add a layout item.
        Inserts a FLEXSPACE item if the previously
        inserted item was a cell.
    */
    void addItem(LayoutItem *item)
    {
        if ((m_insertFlexSpacer) &&
             (item->m_ltype == LayoutItem::TYPE_CELL))
        {
            LayoutItem *flex = new LayoutItem(LayoutItem::TYPE_FLEXSPACE);
            flex->m_size = -1;
            setItemEdgePos(flex);
            m_items.push_back(flex);
        }

        m_items.push_back(item);
        
        if (item->m_ltype == LayoutItem::TYPE_CELL || item->m_ltype == LayoutItem::TYPE_BOND)
        {
            // auto-insert a flex space the next time
            // a regular CELL is inserted.
            //
            // this way, there will always be a flex
            // space between regular cells unless
            // we insert a fixed spacer or offset.
            m_insertFlexSpacer = true;
        }
        else if(item->m_ltype != LayoutItem::TYPE_FILLERDECL) // Excempt the FILLERDECL of changing the insertFlexSpacer state
        {
            m_insertFlexSpacer = false;
        }
    }

    /** set the left-most corner for north and south,
        or bottom most corner for east and west edges.
    */
    void setFirstCorner(LayoutItem *corner)
    {
        m_firstCorner = corner;
        setItemEdgePos(m_firstCorner);
    }

    /** set the right-most corner for north and south,
        or top most corner for east and west edges.
    */
    void setLastCorner(LayoutItem *corner)
    {        
        m_lastCorner = corner;
        setItemEdgePos(m_lastCorner);
    }

    LayoutItem* getFirstCorner() const
    {
        return m_firstCorner;
    }

    LayoutItem* getLastCorner() const
    {
        return m_lastCorner;
    }

    void setEdgePos(double edgePos)
    {
        m_edgePos = edgePos;
        for(auto c : m_items)
        {
            setItemEdgePos(c);
        }
        setItemEdgePos(m_firstCorner);
        setItemEdgePos(m_lastCorner);
    }

    /** get the minimum size of all the items */
    double getMinSize() const;

    /** set grid */
    void setGrid(double grid);

    /** perform the layout */
    bool doLayout();

    /** dump layout */
    void dump();

    typedef std::list<LayoutItem*>::iterator item_iterator;

    /** begin iterator for LayoutItems */
    item_iterator begin() { return m_items.begin(); }

    /** end iterator for LayoutItems */
    item_iterator end() { return m_items.end(); }

    typedef std::list<LayoutItem*>::reverse_iterator item_riterator;

    /** begin iterator for LayoutItems */
    item_riterator rbegin() { return m_items.rbegin(); }

    /** end iterator for LayoutItems */
    item_riterator rend() { return m_items.rend(); }

protected: 
    double getItemPos(const LayoutItem *item) const
    {
        if (m_dir == DIR_HORIZONTAL)
        {
            return item->m_x;
        }
        return item->m_y;
    }

    void setItemPos(LayoutItem *item, double pos)
    {
        if (item == nullptr)
        {
            return;
        }

        if (m_dir == DIR_HORIZONTAL)
        {
            item->m_x = pos;
        }
        else
        {
            item->m_y = pos;
        }
    }

    void setItemEdgePos(LayoutItem *item)
    {
        if (item == nullptr)
        {
            return;
        }

        if (m_dir != DIR_HORIZONTAL)
        {
            item->m_x = m_edgePos;
        }
        else
        {
            item->m_y = m_edgePos;
        }        
    }

    void prepareForLayout();

    bool   m_insertFlexSpacer;
    double m_dieSize;   ///< die size in the direction of layout

    direction_t             m_dir;      ///< direction of layout
    side_t                  m_side;      ///< direction of layout
    double                  m_edgePos;  ///< position of fixed axis of layout
    double                  m_grid;     ///< the grid
    std::list<LayoutItem*>  m_items;    ///< all the cells in the padring
    LayoutItem *m_firstCorner;
    LayoutItem *m_lastCorner;
};

#endif
