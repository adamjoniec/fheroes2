/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2020 - 2025                                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "high_resolution.h"

#include <filesystem>
#include <iomanip>
#include <sstream>

#include "image_palette.h"
#include "image_tool.h"
#include "logging.h"

namespace
{
    int scaleValue = 1;
    std::string packPath;
    bool isCycling( const uint8_t index )
    {
        return ( index >= 214 && index <= 221 ) || ( index >= 231 && index <= 241 );
    }
}
namespace fheroes2
{
    int graphicsScale()
    {
        return scaleValue;
    }
    const std::string & graphicsPack()
    {
        return packPath;
    }
    void configureGraphicsPack( const std::string & path, const int scale )
    {
        packPath = path;
        scaleValue = ( scale >= 2 && scale <= 4 && !path.empty() ) ? scale : 1;
        std::error_code error;
        if ( scaleValue > 1 && !std::filesystem::is_directory( std::filesystem::path( path ), error ) ) {
            ERROR_LOG( "Graphics pack directory does not exist: " << path << ". Using original graphics." )
            scaleValue = 1;
        }
        if ( scaleValue > 1 && !isPNGFormatSupported() ) {
            ERROR_LOG( "High-resolution PNG packs require a build with ENABLE_IMAGE=ON. Using original graphics." )
            scaleValue = 1;
        }
    }
    std::unique_ptr<Image> expandLogicalPixels( const Image & image, const int scale )
    {
        auto detail = std::make_unique<Image>();
        if ( image.singleLayer() )
            detail->_disableTransformLayer();
        detail->resize( image.width() * scale, image.height() * scale );
        if ( image.empty() )
            return detail;
        uint8_t * colors = detail->image();
        uint8_t * effects = detail->singleLayer() ? nullptr : detail->transform();
        const uint8_t * input = image.image();
        const uint8_t * transform = image.singleLayer() ? nullptr : image.transform();
        for ( int y = 0; y < detail->height(); ++y ) {
            for ( int x = 0; x < detail->width(); ++x ) {
                const int from = ( y / scale ) * image.width() + x / scale;
                const int to = y * detail->width() + x;
                colors[to] = input[from];
                if ( effects )
                    effects[to] = transform[from];
            }
        }
        return detail;
    }
    void loadHighResolutionFrame( Image & image, const std::string & type, const std::string & file, const size_t frame, const bool baseGameOnly )
    {
        if ( graphicsScale() == 1 || image.empty() )
            return;
        namespace fs = std::filesystem;
        std::error_code error;
        fs::path root( graphicsPack() );
        if ( fs::is_directory( root / ( "x" + std::to_string( graphicsScale() ) ), error ) )
            root /= "x" + std::to_string( graphicsScale() );
        const std::string name = fs::path( file ).stem().string();
        const fs::path collection = root / "graphics" / ( baseGameOnly ? "Heroes2" : "Heroes2x" ) / type / name;
        std::ostringstream number;
        number << std::setw( 4 ) << std::setfill( '0' ) << frame << ".png";
        const fs::path path = collection / number.str();
        if ( !fs::is_regular_file( path, error ) )
            return;
        auto detail = std::make_unique<Image>();
        if ( !Load( path.string(), *detail ) ) {
            ERROR_LOG( "Ignoring unreadable high-resolution frame: " << path.string() )
            return;
        }
        if ( detail->width() != image.width() * graphicsScale() || detail->height() != image.height() * graphicsScale() ) {
            ERROR_LOG( "Ignoring high-resolution frame with incompatible dimensions: " << path.string() )
            return;
        }
        // Preserve original effect types, rather than interpreting approximate RGBA shadows.
        const auto reference = expandLogicalPixels( image, graphicsScale() );
        uint8_t * output = detail->image();
        uint8_t * effects = detail->transform();
        const uint8_t * indices = static_cast<const Image &>( *reference ).image();
        const uint8_t * masks = reference->singleLayer() ? nullptr : static_cast<const Image &>( *reference ).transform();
        const RGB * palette = getRGBGamePalette();
        for ( int i = 0; i < detail->width() * detail->height(); ++i ) {
            effects[i] = masks ? masks[i] : 0;
            const auto & a = palette[output[i]];
            const auto & b = palette[indices[i]];
            if ( isCycling( indices[i] ) || ( a.r == b.r && a.g == b.g && a.b == b.b ) )
                output[i] = indices[i];
        }
        if ( image.singleLayer() )
            detail->_disableTransformLayer();
        image.setHighResolution( std::move( detail ) );
    }
}
