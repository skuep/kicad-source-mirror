/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2015 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 2012 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
 * Copyright (C) 1992-2016 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

/**
 * @file class_module.cpp
 * @brief TEXT_MODULE class implementation.
 */

#include <fctsys.h>
#include <gr_basic.h>
#include <wxstruct.h>
#include <trigo.h>
#include <class_drawpanel.h>
#include <drawtxt.h>
#include <kicad_string.h>
#include <colors_selection.h>
#include <richio.h>
#include <macros.h>
#include <wxBasePcbFrame.h>
#include <msgpanel.h>
#include <base_units.h>
#include <bitmaps.h>

#include <class_board.h>
#include <class_module.h>

#include <view/view.h>

#include <pcbnew.h>


TEXTE_MODULE::TEXTE_MODULE( MODULE* parent, TEXT_TYPE text_type ) :
    BOARD_ITEM( parent, PCB_MODULE_TEXT_T ),
    EDA_TEXT()
{
    MODULE* module = static_cast<MODULE*>( m_Parent );

    m_Type = text_type;

    // Set text thickness to a default value
    SetThickness( Millimeter2iu( 0.15 ) );
    SetLayer( F_SilkS );

    // Set position and give a default layer if a valid parent footprint exists
    if( module && ( module->Type() == PCB_MODULE_T ) )
    {
        SetTextPos( module->GetPosition() );

        if( IsBackLayer( module->GetLayer() ) )
        {
            SetLayer( B_SilkS );
            SetMirrored( true );
        }
    }

    SetDrawCoord();
}


TEXTE_MODULE::~TEXTE_MODULE()
{
}


void TEXTE_MODULE::SetTextAngle( double aAngle )
{
    EDA_TEXT::SetTextAngle( NormalizeAngle360( aAngle ) );
}


bool TEXTE_MODULE::TextHitTest( const wxPoint& aPoint, int aAccuracy ) const
{
    EDA_RECT rect = GetTextBox( -1 );
    wxPoint location = aPoint;

    rect.Inflate( aAccuracy );

    RotatePoint( &location, GetTextPos(), -GetDrawRotation() );

    return rect.Contains( location );
}


bool TEXTE_MODULE::TextHitTest( const EDA_RECT& aRect, bool aContains, int aAccuracy ) const
{
    EDA_RECT rect = aRect;

    rect.Inflate( aAccuracy );

    if( aContains )
    {
        return rect.Contains( GetBoundingBox() );
    }
    else
    {
        return rect.Intersects( GetTextBox( -1 ), GetDrawRotation() );
    }
}


void TEXTE_MODULE::Rotate( const wxPoint& aRotCentre, double aAngle )
{
    // Used in footprint edition
    // Note also in module editor, m_Pos0 = m_Pos

    wxPoint pt = GetTextPos();
    RotatePoint( &pt, aRotCentre, aAngle );
    SetTextPos( pt );

    SetTextAngle( GetTextAngle() + aAngle );
    SetLocalCoord();
}


void TEXTE_MODULE::Flip( const wxPoint& aCentre )
{
    // flipping the footprint is relative to the X axis
    SetTextY( ::Mirror( GetTextPos().y, aCentre.y ) );

    SetTextAngle( -GetTextAngle() );

    SetLayer( FlipLayer( GetLayer() ) );
    SetMirrored( IsBackLayer( GetLayer() ) );
    SetLocalCoord();
}


void TEXTE_MODULE::Mirror( const wxPoint& aCentre, bool aMirrorAroundXAxis )
{
    // Used in modedit, to transform the footprint
    // the mirror is around the Y axis or X axis if aMirrorAroundXAxis = true
    // the position is mirrored, but the text itself is not mirrored
    if( aMirrorAroundXAxis )
        SetTextY( ::Mirror( GetTextPos().y, aCentre.y ) );
    else
        SetTextX( ::Mirror( GetTextPos().x, aCentre.x ) );

    SetLocalCoord();
}


void TEXTE_MODULE::Move( const wxPoint& aMoveVector )
{
    Offset( aMoveVector );
    SetLocalCoord();
}


int TEXTE_MODULE::GetLength() const
{
    return m_Text.Len();
}


void TEXTE_MODULE::SetDrawCoord()
{
    const MODULE* module = static_cast<const MODULE*>( m_Parent );

    SetTextPos( m_Pos0 );

    if( module  )
    {
        double angle = module->GetOrientation();

        wxPoint pt = GetTextPos();
        RotatePoint( &pt, angle );
        SetTextPos( pt );

        Offset( module->GetPosition() );
    }
}


void TEXTE_MODULE::SetLocalCoord()
{
    const MODULE* module = static_cast<const MODULE*>( m_Parent );

    if( module )
    {
        m_Pos0 = GetTextPos() - module->GetPosition();

        double angle = module->GetOrientation();

        RotatePoint( &m_Pos0.x, &m_Pos0.y, -angle );
    }
    else
    {
        m_Pos0 = GetTextPos();
    }
}

const EDA_RECT TEXTE_MODULE::GetBoundingBox() const
{
    double   angle = GetDrawRotation();
    EDA_RECT text_area = GetTextBox( -1, -1 );

    if( angle )
        text_area = text_area.GetBoundingBoxRotated( GetTextPos(), angle );

    return text_area;
}


void TEXTE_MODULE::Draw( EDA_DRAW_PANEL* aPanel, wxDC* aDC, GR_DRAWMODE aDrawMode,
                         const wxPoint& aOffset )
{
    if( aPanel == NULL )
        return;

    /* parent must *not* be NULL (a footprint text without a footprint
       parent has no sense) */
    wxASSERT( m_Parent );

    BOARD* brd = GetBoard( );
    COLOR4D color = brd->GetLayerColor( GetLayer() );
    PCB_LAYER_ID text_layer = GetLayer();

    if( !brd->IsLayerVisible( m_Layer )
      || ( IsFrontLayer( text_layer ) && !brd->IsElementVisible( LAYER_MOD_TEXT_FR ) )
      || ( IsBackLayer( text_layer ) && !brd->IsElementVisible( LAYER_MOD_TEXT_BK ) ) )
        return;

    // Invisible texts are still drawn (not plotted) in LAYER_MOD_TEXT_INVISIBLE
    // Just because we must have to edit them (at least to make them visible)
    if( !IsVisible() )
    {
        if( !brd->IsElementVisible( LAYER_MOD_TEXT_INVISIBLE ) )
            return;

        color = brd->GetVisibleElementColor( LAYER_MOD_TEXT_INVISIBLE );
    }

    DISPLAY_OPTIONS* displ_opts = (DISPLAY_OPTIONS*)aPanel->GetDisplayOptions();

    // shade text if high contrast mode is active
    if( ( aDrawMode & GR_ALLOW_HIGHCONTRAST ) && displ_opts && displ_opts->m_ContrastModeDisplay )
    {
        PCB_LAYER_ID curr_layer = ( (PCB_SCREEN*) aPanel->GetScreen() )->m_Active_Layer;

        if( !IsOnLayer( curr_layer ) )
            color = COLOR4D( DARKDARKGRAY );
    }

    // Draw mode compensation for the width
    int width = GetThickness();

    if( displ_opts && displ_opts->m_DisplayModTextFill == SKETCH )
        width = -width;

    GRSetDrawMode( aDC, aDrawMode );
    wxPoint pos = GetTextPos() - aOffset;

    // Draw the text anchor point
    if( brd->IsElementVisible( LAYER_ANCHOR ) )
    {
        COLOR4D anchor_color = brd->GetVisibleElementColor( LAYER_ANCHOR );
        GRDrawAnchor( aPanel->GetClipBox(), aDC, pos.x, pos.y, DIM_ANCRE_TEXTE, anchor_color );
    }

    // Draw the text proper, with the right attributes
    wxSize size   = GetTextSize();
    double orient = GetDrawRotation();

    // If the text is mirrored : negate size.x (mirror / Y axis)
    if( IsMirrored() )
        size.x = -size.x;

    DrawGraphicText( aPanel->GetClipBox(), aDC, pos, color, GetShownText(), orient,
                     size, GetHorizJustify(), GetVertJustify(),
                     width, IsItalic(), IsBold() );

    // Enable these line to draw the bounding box (debug test purpose only)
#if 0
    {
        EDA_RECT BoundaryBox = GetBoundingBox();
        GRRect( aPanel->GetClipBox(), aDC, BoundaryBox, 0, BROWN );
    }
#endif
}


void TEXTE_MODULE::DrawUmbilical( EDA_DRAW_PANEL* aPanel,
                                  wxDC*           aDC,
                                  GR_DRAWMODE     aDrawMode,
                                  const wxPoint&  aOffset )
{
    MODULE* parent = static_cast<MODULE*>( GetParent() );

    if( !parent )
        return;

    GRSetDrawMode( aDC, GR_XOR );
    GRLine( aPanel->GetClipBox(), aDC,
            parent->GetPosition(), GetTextPos() + aOffset,
            0, UMBILICAL_COLOR);
}


double TEXTE_MODULE::GetDrawRotation() const
{
    MODULE* module = (MODULE*) m_Parent;
    double  rotation = GetTextAngle();

    if( module )
        rotation += module->GetOrientation();

    // Keep angle between -90 .. 90 deg. Otherwise the text is not easy to read
    while( rotation > 900 )
        rotation -= 1800;

    while( rotation < -900 )
        rotation += 1800;

    return rotation;
}


// see class_text_mod.h
void TEXTE_MODULE::GetMsgPanelInfo( std::vector< MSG_PANEL_ITEM >& aList )
{
    MODULE* module = (MODULE*) m_Parent;

    if( module == NULL )        // Happens in modedit, and for new texts
        return;

    wxString msg, Line;

    static const wxString text_type_msg[3] =
    {
        _( "Ref." ), _( "Value" ), _( "Text" )
    };

    Line = module->GetReference();
    aList.push_back( MSG_PANEL_ITEM( _( "Footprint" ), Line, DARKCYAN ) );

    Line = GetShownText();
    aList.push_back( MSG_PANEL_ITEM( _( "Text" ), Line, BROWN ) );

    wxASSERT( m_Type >= TEXT_is_REFERENCE && m_Type <= TEXT_is_DIVERS );
    aList.push_back( MSG_PANEL_ITEM( _( "Type" ), text_type_msg[m_Type], DARKGREEN ) );

    if( !IsVisible() )
        msg = _( "No" );
    else
        msg = _( "Yes" );

    aList.push_back( MSG_PANEL_ITEM( _( "Display" ), msg, DARKGREEN ) );

    // Display text layer
    aList.push_back( MSG_PANEL_ITEM( _( "Layer" ), GetLayerName(), DARKGREEN ) );

    if( IsMirrored() )
        msg = _( " Yes" );
    else
        msg = _( " No" );

    aList.push_back( MSG_PANEL_ITEM( _( "Mirror" ), msg, DARKGREEN ) );

    msg.Printf( wxT( "%.1f" ), GetTextAngleDegrees() );
    aList.push_back( MSG_PANEL_ITEM( _( "Angle" ), msg, DARKGREEN ) );

    msg = ::CoordinateToString( GetThickness() );
    aList.push_back( MSG_PANEL_ITEM( _( "Thickness" ), msg, DARKGREEN ) );

    msg = ::CoordinateToString( GetTextWidth() );
    aList.push_back( MSG_PANEL_ITEM( _( "Width" ), msg, RED ) );

    msg = ::CoordinateToString( GetTextHeight() );
    aList.push_back( MSG_PANEL_ITEM( _( "Height" ), msg, RED ) );
}


wxString TEXTE_MODULE::GetSelectMenuText() const
{
    wxString text;
    const wxChar *reference = GetChars( static_cast<MODULE*>( GetParent() )->GetReference() );

    switch( m_Type )
    {
    case TEXT_is_REFERENCE:
        text.Printf( _( "Reference %s" ), reference );
        break;

    case TEXT_is_VALUE:
        text.Printf( _( "Value %s of %s" ), GetChars( GetShownText() ), reference );
        break;

    default:    // wrap this one in quotes:
        text.Printf( _( "Text \"%s\" on %s of %s" ), GetChars( ShortenedShownText() ),
                     GetChars( GetLayerName() ), reference );
        break;
    }

    return text;
}


BITMAP_DEF TEXTE_MODULE::GetMenuImage() const
{
    return footprint_text_xpm;
}


EDA_ITEM* TEXTE_MODULE::Clone() const
{
    return new TEXTE_MODULE( *this );
}


const BOX2I TEXTE_MODULE::ViewBBox() const
{
    double   angle = GetDrawRotation();
    EDA_RECT text_area = GetTextBox( -1, -1 );

    if( angle )
        text_area = text_area.GetBoundingBoxRotated( GetTextPos(), angle );

    return BOX2I( text_area.GetPosition(), text_area.GetSize() );
}


void TEXTE_MODULE::ViewGetLayers( int aLayers[], int& aCount ) const
{
    if( !IsVisible() )      // Hidden text
        aLayers[0] = LAYER_MOD_TEXT_INVISIBLE;
    //else if( IsFrontLayer( m_Layer ) )
        //aLayers[0] = LAYER_MOD_TEXT_FR;
    //else if( IsBackLayer( m_Layer ) )
        //aLayers[0] = LAYER_MOD_TEXT_BK;
    else
        aLayers[0] = GetLayer();

    aCount = 1;
}


unsigned int TEXTE_MODULE::ViewGetLOD( int aLayer, KIGFX::VIEW* aView ) const
{
    const int MAX = std::numeric_limits<unsigned int>::max();

    if( !aView )
        return 0;

    if( m_Type == TEXT_is_VALUE && !aView->IsLayerVisible( LAYER_MOD_VALUES ) )
        return MAX;

    if( m_Type == TEXT_is_REFERENCE && !aView->IsLayerVisible( LAYER_MOD_REFERENCES ) )
        return MAX;

    if( IsFrontLayer( m_Layer ) && ( !aView->IsLayerVisible( LAYER_MOD_TEXT_FR ) ||
                                     !aView->IsLayerVisible( LAYER_MOD_FR ) ) )
        return MAX;

    if( IsBackLayer( m_Layer ) && ( !aView->IsLayerVisible( LAYER_MOD_TEXT_BK ) ||
                                    !aView->IsLayerVisible( LAYER_MOD_BK ) ) )
        return MAX;

    return 0;
}


wxString TEXTE_MODULE::GetShownText() const
{
    /* First order optimization: no % means that no processing is
     * needed; just hope that RVO and copy constructor implementation
     * avoid to copy the whole block; anyway it should be better than
     * rebuild the string one character at a time...
     * Also it seems wise to only expand macros in user text (but there
     * is no technical reason, probably) */

    if( (m_Type != TEXT_is_DIVERS) || (wxString::npos == m_Text.find('%')) )
        return m_Text;

    wxString newbuf;
    const MODULE *module = static_cast<MODULE*>( GetParent() );

    for( wxString::const_iterator it = m_Text.begin(); it != m_Text.end(); ++it )
    {
        // Process '%' and copy everything else
        if( *it != '%' )
            newbuf.append(*it);
        else
        {
            /* Look at the next character (if is it there) and append
             * its expansion */
            ++it;

            if( it != m_Text.end() )
            {
                switch( char(*it) )
                {
                case '%':
                    newbuf.append( '%' );
                    break;

                case 'R':
                    if( module )
                        newbuf.append( module->GetReference() );
                    break;

                case 'V':
                    if( module )
                        newbuf.append( module->GetValue() );
                    break;

                default:
                    newbuf.append( '?' );
                    break;
                }
            }
            else
                break; // The string is over and we can't ++ anymore
        }
    }

    return newbuf;
}
