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

#ifdef NDEBUG
#undef NDEBUG
#endif
#include "agg_file.h"
#include "high_resolution.h"
#include "image_palette.h"
#include "image_tool.h"
#include "screen.h"
#include "serialize.h"
#include "smk_decoder.h"
#define SDL_MAIN_HANDLED
#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <iostream>

#include <SDL.h>
#if defined( _WIN32 )
#include <stdlib.h>
#include <windows.h>
#endif
#include <numeric>

using namespace fheroes2;

static void samePixels( const Image & a, const Image & b )
{
    assert( a.width() == b.width() && a.height() == b.height() );
    assert( std::memcmp( a.image(), b.image(), static_cast<size_t>( a.width() ) * a.height() ) == 0 );
}

static Image fixture( const int scale )
{
    Image image( 2, 2 );
    image.fill( 10 );
    image.transform()[3] = 1;
    auto detail = expandLogicalPixels( image, scale );
    uint8_t * colors = detail->image();
    for ( int y = 0; y < scale * 2; ++y )
        for ( int x = 0; x < scale * 2; ++x )
            colors[y * scale * 2 + x] = static_cast<uint8_t>( 20 + y * scale * 2 + x );
    image.setHighResolution( std::move( detail ) );
    return image;
}

static void operations( const int scale )
{
    configureGraphicsPack( std::filesystem::temp_directory_path().string(), scale );
    Image input = fixture( scale );
    Image copy( input );
    assert( copy.highResolution() != input.highResolution() );
    samePixels( *copy.highResolution(), *input.highResolution() );
    Image output( 5, 4 );
    output._disableTransformLayer();
    output.fill( 0 );
    Image expected( 5 * scale, 4 * scale );
    expected._disableTransformLayer();
    expected.fill( 0 );
    Blit( *input.highResolution(), 0, 0, expected, -scale, scale, 2 * scale, 2 * scale );
    Blit( input, 0, 0, output, -1, 1, 2, 2 );
    assert( output.width() == 5 && output.height() == 4 && output.highResolution() );
    samePixels( *output.highResolution(), expected );
    Blit( *input.highResolution(), expected, 2 * scale, scale, true );
    Blit( input, output, 2, 1, true );
    samePixels( *output.highResolution(), expected );
    const Sprite cropped = Crop( input, 1, 0, 1, 2 );
    assert( cropped.x() == 1 && cropped.highResolution()->width() == scale );
    const Sprite expectedCrop = Crop( *input.highResolution(), scale, 0, scale, scale * 2 );
    samePixels( *cropped.highResolution(), expectedCrop );
    std::vector<uint8_t> palette( 256 );
    std::iota( palette.begin(), palette.end(), 0 );
    palette[20] = 77;
    ApplyPalette( output, palette );
    ApplyPalette( expected, palette );
    samePixels( *output.highResolution(), expected );
    Fill( output, 1, 2, 2, 1, 88 );
    Fill( expected, scale, 2 * scale, 2 * scale, scale, 88 );
    samePixels( *output.highResolution(), expected );
    AlphaBlit( input, 0, 0, output, 1, 1, 2, 2, 128, true );
    AlphaBlit( *input.highResolution(), 0, 0, expected, scale, scale, 2 * scale, 2 * scale, 128, true );
    samePixels( *output.highResolution(), expected );
    DrawLine( output, { 0, 0 }, { 3, 2 }, 44 );
    assert( output.highResolution() );
    DrawBorder( output, 45 );
    assert( output.highResolution() );
    const Image snapshot = output;
    {
        ImageRestorer restore( output );
        Fill( output, 0, 0, 5, 4, 15 );
    }
    samePixels( *output.highResolution(), *snapshot.highResolution() );
    Image resized( 4, 4 );
    Resize( input, resized );
    assert( resized.highResolution() && resized.highResolution()->width() == 4 * scale );
    Image transposed( 4, 5 );
    Transpose( output, transposed );
    assert( transposed.highResolution() && transposed.width() == 4 && transposed.height() == 5 );
    output.image()[0] = 1;
    assert( !output.highResolution() ); // Direct legacy writes cannot leave stale detail.
    configureGraphicsPack( "", 1 );
    Image originalOnly( 5, 4 );
    originalOnly.fill( 0 );
    Blit( input, originalOnly );
    assert( !originalOnly.highResolution() );
}

static void realAssets( const std::filesystem::path & root, const int scale )
{
    AGGFile archive;
    assert( archive.open( ( root / "data/Heroes2.agg" ).string() ) );
    setGamePalette( archive.read( "KB.PAL" ) );
    const auto data = archive.read( "PEASANT.ICN" );
    ROStreamBuf stream( data );
    stream.seek( 6 + 13 );
    ICNHeader header, next;
    stream >> header >> next;
    Sprite image = decodeICNSprite( data.data() + 6 + header.offsetData, data.data() + 6 + next.offsetData, header );
    configureGraphicsPack( ( root / "upscaled/pixel" ).string(), scale );
    loadHighResolutionFrame( image, "icn", "PEASANT.ICN", 1 );
    assert( image.highResolution() && image.width() == header.width && image.x() == header.offsetX );
    const Image & logical = image;
    const Image & detail = *image.highResolution();
    bool subpixelDetail = false;
    for ( int y = 0; y < detail.height(); ++y )
        for ( int x = 0; x < detail.width(); ++x ) {
            assert( detail.transform()[y * detail.width() + x] == logical.transform()[( y / scale ) * image.width() + x / scale] );
            if ( x % scale && detail.image()[y * detail.width() + x] != detail.image()[y * detail.width() + x - 1] )
                subpixelDetail = true;
        }
    assert( subpixelDetail );
    const auto terrain = archive.read( "GROUND32.TIL" );
    Image tile( 32, 32 );
    tile._disableTransformLayer();
    std::memcpy( tile.image(), terrain.data() + 6, 32 * 32 );
    loadHighResolutionFrame( tile, "til", "GROUND32.TIL", 0 );
    assert( tile.highResolution() && tile.highResolution()->singleLayer() );
    const Image flipped = Flip( tile, true, true );
    const Image flippedDetail = Flip( *tile.highResolution(), true, true );
    samePixels( *flipped.highResolution(), flippedDetail );
    Sprite bitmap = decodeBMPFile( archive.read( "ADVMBW01.BMP" ) );
    loadHighResolutionFrame( bitmap, "bmp", "ADVMBW01.BMP", 0 );
    assert( bitmap.highResolution() );
    AGGFile expansion;
    assert( expansion.open( ( root / "data/Heroes2x.agg" ).string() ) && expansion.contains( "HEROES.ICN" ) );
    const auto expanded = expansion.read( "HEROES.ICN" );
    ROStreamBuf expandedStream( expanded );
    expandedStream.seek( 6 );
    ICNHeader expandedHeader;
    expandedStream >> expandedHeader;
    Sprite expandedImage = decodeICNSprite( expanded.data() + 6 + expandedHeader.offsetData, expanded.data() + expanded.size(), expandedHeader );
    loadHighResolutionFrame( expandedImage, "icn", "HEROES.ICN", 0, false );
    assert( expandedImage.highResolution() );
    Image missing( 2, 2 );
    missing.fill( 0 );
    loadHighResolutionFrame( missing, "icn", "MISSING.ICN", 0 );
    assert( !missing.highResolution() );
    Image wrongSize( 1, 1 );
    wrongSize.fill( 0 );
    loadHighResolutionFrame( wrongSize, "icn", "PEASANT.ICN", 1 );
    assert( !wrongSize.highResolution() );
    SMKVideoSequence video( ( root / "data/Ivydes.smk" ).string() );
    Image canvas( video.width(), video.height() );
    canvas._disableTransformLayer();
    canvas.fill( 0 );
    int width = 0, height = 0;
    std::vector<uint8_t> colors;
    video.getNextFrame( canvas, 0, 0, width, height, colors );
    assert( canvas.highResolution() && width == video.width() && height == video.height() && video.getCurrentFrameId() == 1 );
    const Image first = *canvas.highResolution();
    video.resetFrame();
    video.getNextFrame( canvas, 0, 0, width, height, colors );
    samePixels( first, *canvas.highResolution() );
}

static void renderer()
{
    SDL_setenv( "SDL_VIDEODRIVER", "dummy", 1 );
    SDL_setenv( "SDL_RENDER_DRIVER", "software", 1 );
    SDL_SetMainReady();
    assert( SDL_Init( SDL_INIT_VIDEO ) == 0 );
    auto & display = Display::instance();
    for ( const int scale : { 2, 3, 4 } ) {
        configureGraphicsPack( std::filesystem::temp_directory_path().string(), scale );
        display.setResolution( { 640, 480 } );
        display.changePalette( nullptr, true );
        SDL_Window * window = nullptr;
        for ( unsigned id = 1; id < 10 && !window; ++id )
            window = SDL_GetWindowFromID( id );
        assert( window );
        SDL_Renderer * target = SDL_GetRenderer( window );
        assert( target );
        display.fill( 0 );
        Image source( 2, 2 );
        source.fill( 0 );
        auto detailed = expandLogicalPixels( source, scale );
        detailed->fill( 11 );
        source.setHighResolution( std::move( detailed ) );
        Blit( source, display, 10, 10 );
        assert( display.highResolution() );
        display.render();
        int width = 0, height = 0;
        SDL_GetRendererOutputSize( target, &width, &height );
        std::vector<uint32_t> pixels( static_cast<size_t>( width ) * height );
        assert( SDL_RenderReadPixels( target, nullptr, SDL_PIXELFORMAT_RGB888, pixels.data(), width * 4 ) == 0 );
        assert( std::any_of( pixels.begin(), pixels.end(), []( const uint32_t color ) { return ( color & 0xffffff ) != 0; } ) );
    }
    display.release();
    SDL_Quit();
}

int main( const int argc, const char ** argv )
{
#if defined( _WIN32 )
    SetErrorMode( SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX );
    _set_error_mode( _OUT_TO_STDERR );
    _set_abort_behavior( 0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT );
#endif
    std::cout << std::unitbuf;
    for ( const int scale : { 2, 3, 4 } ) {
        std::cout << "Testing scale " << scale << " operations\n";
        operations( scale );
        if ( argc > 1 && std::strlen( argv[1] ) )
            realAssets( argv[1], scale );
        std::cout << "PASS: scale " << scale << " image operations and assets\n";
    }
    renderer();
    std::cout << "PASS: headless SDL high-resolution presentation\n";
}
