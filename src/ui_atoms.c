/*
Copyright (C) 2003-2006 Andrey Nazarov

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "ui_local.h"

uiStatic_t    uis;

LIST_DECL( ui_menus );

cvar_t    *ui_debug;
static cvar_t    *ui_open;
static cvar_t    *ui_scale;

// ===========================================================================

/*
=================
UI_PushMenu
=================
*/
void UI_PushMenu( menuFrameWork_t *menu ) {
    int i, j;

    if( !menu ) {
        return;
    }

    // if this menu is already present, drop back to that level
    // to avoid stacking menus by hotkeys
    for( i = 0; i < uis.menuDepth; i++ ) {
        if( uis.layers[i] == menu ) {
            break;
        }
    }

    if( i == uis.menuDepth ) {
        if( uis.menuDepth >= MAX_MENU_DEPTH ) {
            Com_EPrintf( "UI_PushMenu: MAX_MENU_DEPTH exceeded\n" );
            return;
        }
        uis.layers[uis.menuDepth++] = menu;
    } else {
        for( j = i; j < uis.menuDepth; j++ ) {
            UI_PopMenu();
        }
        uis.menuDepth = i + 1;
    }

    if( menu->push ) {
        if( !menu->push( menu ) ) {
            return;
        }
    }

    Menu_Init( menu );

    Con_Close();

    Key_SetDest( ( Key_GetDest() & ~KEY_CONSOLE ) | KEY_MENU );

    if( !uis.activeMenu ) {
        uis.entersound = qtrue;
        IN_WarpMouse( 0, menu->y1 );
    }

    uis.transparent |= menu->transparent;
    uis.activeMenu = menu;

    UI_DoHitTest();

    if( menu->expose ) {
        menu->expose( menu );
    }
}

static void UI_Resize( void ) {
    int i;

#if USE_REF == REF_SOFT
    uis.clipRect.left = 0;
    uis.clipRect.top = 0;
    uis.clipRect.right = uis.glconfig.vidWidth;
    uis.clipRect.bottom = uis.glconfig.vidHeight;
    uis.scale = 1;
    uis.width = uis.glconfig.vidWidth;
    uis.height = uis.glconfig.vidHeight;
#else
    Cvar_ClampValue( ui_scale, 1, 9 );
    uis.scale = 1 / ui_scale->value;
    uis.width = uis.glconfig.vidWidth * uis.scale;
    uis.height = uis.glconfig.vidHeight * uis.scale;
#endif

    for( i = 0; i < uis.menuDepth; i++ ) {
        Menu_Init( uis.layers[i] );
    }

    //CL_WarpMouse( 0, 0 );
}


/*
=================
UI_ForceMenuOff
=================
*/
void UI_ForceMenuOff( void ) {
    menuFrameWork_t *menu;
    int i;

    for( i = 0; i < uis.menuDepth; i++ ) {
        menu = uis.layers[i];
        if( menu->pop ) {
            menu->pop( menu );
        }
    }

    Key_SetDest( Key_GetDest() & ~KEY_MENU );
    uis.menuDepth = 0;
    uis.activeMenu = NULL;
    uis.transparent = qfalse;
}

/*
=================
UI_PopMenu
=================
*/
void UI_PopMenu( void ) {
    menuFrameWork_t *menu;
    int i;

    if( uis.menuDepth < 1 )
        Com_Error( ERR_FATAL, "UI_PopMenu: depth < 1" );

    menu = uis.layers[--uis.menuDepth];
    if( menu->pop ) {
        menu->pop( menu );
    }

    if( !uis.menuDepth ) {
        UI_ForceMenuOff();
        return;
    }

    uis.activeMenu = uis.layers[uis.menuDepth - 1];

    uis.transparent = qfalse;
    for( i = uis.menuDepth - 1; i >= 0; i-- ) {
        if( uis.layers[i]->transparent ) {
            uis.transparent = qtrue;
            break;
        }
    }

    UI_DoHitTest();
}

/*
=================
UI_IsTransparent
=================
*/
qboolean UI_IsTransparent( void ) {
    if( !( Key_GetDest() & KEY_MENU ) ) {
        return qtrue;
    }

    if( !uis.activeMenu ) {
        return qtrue;
    }

    return uis.transparent;
}

menuFrameWork_t *UI_FindMenu( const char *name ) {
    menuFrameWork_t *menu;

    LIST_FOR_EACH( menuFrameWork_t, menu, &ui_menus, entry ) {
        if( !strcmp( menu->name, name ) ) {
            return menu;
        }
    }
    return NULL;
}

/*
=================
UI_OpenMenu
=================
*/
void UI_OpenMenu( uiMenu_t type ) {
    menuFrameWork_t *menu = NULL;

    if( !uis.initialized ) {
        return;
    }

    // close any existing menus
    UI_ForceMenuOff();

    switch( type ) {
    case UIMENU_MAIN:
        if( ui_open->integer ) {
            menu = UI_FindMenu( "main" );
        }
        break;
    case UIMENU_MAIN_FORCE:
        menu = UI_FindMenu( "main" );
        break;
    case UIMENU_INGAME:
        menu = UI_FindMenu( "game" );
        break;
    case UIMENU_NONE:
        break;
    default:
        Com_Error( ERR_FATAL, "UI_OpenMenu: bad menu" );
        break;
    }

    UI_PushMenu( menu );
}

void UI_ErrorMenu( error_type_t type, const char *text ) {
    if( !uis.initialized ) {
        return;
    }
    // close any existing menus
    UI_ForceMenuOff();

    if( ui_open->integer ) {
        UI_PushMenu( UI_FindMenu( "main" ) );
        M_Menu_Error( type, text );
    }
}


//=============================================================================

/*
=================
UI_FormatColumns
=================
*/
void *UI_FormatColumns( int extrasize, ... ) {
    va_list argptr;
    char *buffer, *p;
    int i, j;
    size_t total = 0;
    char *strings[MAX_COLUMNS];
    size_t lengths[MAX_COLUMNS];

    va_start( argptr, extrasize );
    for( i = 0; i < MAX_COLUMNS; i++ ) {
        if( ( p = va_arg( argptr, char * ) ) == NULL ) {
            break;
        }
        strings[i] = p;
        total += lengths[i] = strlen( p ) + 1;
    }
    va_end( argptr );

    buffer = UI_Malloc( extrasize + total + 1 );
    p = buffer + extrasize;
    for( j = 0; j < i; j++ ) {
        memcpy( p, strings[j], lengths[j] );
        p += lengths[j];
    }
    *p = 0;

    return buffer;
}

char *UI_GetColumn( char *s, int n ) {
    int i;

    for( i = 0; i < n && *s; i++ ) {
        s += strlen( s ) + 1;
    }

    return s;
}

/*
=================
UI_CursorInRect
=================
*/
qboolean UI_CursorInRect( vrect_t *rect ) {
    if( uis.mouseCoords[0] < rect->x ) {
        return qfalse;
    }
    if( uis.mouseCoords[0] >= rect->x + rect->width ) {
        return qfalse;
    }
    if( uis.mouseCoords[1] < rect->y ) {
        return qfalse;
    }
    if( uis.mouseCoords[1] >= rect->y + rect->height ) {
        return qfalse;
    }
    return qtrue;
}

void UI_DrawString( int x, int y, const color_t color, int flags, const char *string ) {
    if( color ) {
        R_SetColor( DRAW_COLOR_RGBA, color );
    }
    
    if( ( flags & UI_CENTER ) == UI_CENTER ) {
        x -= strlen( string ) * 8 / 2;
    } else if( flags & UI_RIGHT ) {
        x -= strlen( string ) * 8;
    }
    
    R_DrawString( x, y, flags, MAX_STRING_CHARS, string, uis.fontHandle );
    if( color ) {
        R_SetColor( DRAW_COLOR_CLEAR, NULL );
    }
}

void UI_DrawChar( int x, int y, int flags, int ch ) {
    R_DrawChar( x, y, flags, ch, uis.fontHandle );
}

void UI_StringDimensions( vrect_t *rc, int flags, const char *string ) {
    rc->height = 8;
    rc->width = 8 * strlen( string );
    
    if( ( flags & UI_CENTER ) == UI_CENTER ) {
        rc->x -= rc->width / 2;
    } else if( flags & UI_RIGHT ) {
        rc->x -= rc->width;
    }    
}

void UI_DrawRect( const vrect_t *rc, int border, int color ) {
    R_DrawFill( rc->x, rc->y, border, rc->height, color ); // left
    R_DrawFill( rc->x + rc->width - border, rc->y, border, rc->height, color ); // right
    R_DrawFill( rc->x + border, rc->y, rc->width - border * 2, border, color ); // top
    R_DrawFill( rc->x + border, rc->y + rc->height - border, rc->width - border * 2, border, color ); // bottom
}

void UI_DrawRectEx( const vrect_t *rc, int border, const color_t color ) {
    R_DrawFillEx( rc->x, rc->y, border, rc->height, color ); // left
    R_DrawFillEx( rc->x + rc->width - border, rc->y, border, rc->height, color ); // right
    R_DrawFillEx( rc->x + border, rc->y, rc->width - border * 2, border, color ); // top
    R_DrawFillEx( rc->x + border, rc->y + rc->height - border, rc->width - border * 2, border, color ); // bottom
}


//=============================================================================
/* Menu Subsystem */

/*
=================
UI_DoHitTest
=================
*/
qboolean UI_DoHitTest( void ) {
    menuCommon_t *item;

    if( !uis.activeMenu ) {
        return qfalse;
    }

    if( !( item = Menu_HitTest( uis.activeMenu ) ) ) {
        return qfalse;
    }
    if( !UI_IsItemSelectable( item ) ) {
        return qfalse;
    }

    Menu_MouseMove( item );

    if( item->flags & QMF_HASFOCUS ) {
        return qfalse;
    }

    Menu_SetFocus( item );
    
    return qtrue;
}

/*
=================
UI_MouseEvent
=================
*/
void UI_MouseEvent( int x, int y ) {
    clamp( x, 0, uis.glconfig.vidWidth );
    clamp( y, 0, uis.glconfig.vidHeight );

    uis.mouseCoords[0] = x * uis.scale;
    uis.mouseCoords[1] = y * uis.scale;

    UI_DoHitTest();
}

/*
=================
UI_Draw
=================
*/
void UI_Draw( int realtime ) {
    int i;

    uis.realtime = realtime;

    if( !( Key_GetDest() & KEY_MENU ) ) {
        return;
    }

    if( !uis.activeMenu ) {
        return;
    }

    R_SetColor( DRAW_COLOR_CLEAR, NULL );
#if USE_REF == REF_SOFT
    R_SetClipRect( DRAW_CLIP_MASK, &uis.clipRect );
#else
    R_SetScale( &uis.scale );
#endif

    if( !uis.transparent ) {
        // draw top menu
        if( uis.activeMenu->draw ) {
            uis.activeMenu->draw( uis.activeMenu );
        } else {
            Menu_Draw( uis.activeMenu );
        }
    } else {
        // draw all layers
        for( i = 0; i < uis.menuDepth; i++ ) {
            if( uis.layers[i]->draw ) {
                uis.layers[i]->draw( uis.layers[i] );
            } else {
                Menu_Draw( uis.layers[i] );
            }
        }
    }

    // draw custom cursor in fullscreen mode
    if( uis.glconfig.flags & QVF_FULLSCREEN ) {
        R_DrawPic( uis.mouseCoords[0] - uis.cursorWidth / 2,
            uis.mouseCoords[1] - uis.cursorHeight / 2, uis.cursorHandle );
    }

    if( ui_debug->integer ) {
        UI_DrawString( uis.width - 4, 4, NULL, UI_RIGHT,
            va( "%3i %3i", uis.mouseCoords[0], uis.mouseCoords[1] ) );
    }

    // delay playing the enter sound until after the
    // menu has been drawn, to avoid delay while
    // caching images
    if( uis.entersound ) {
        uis.entersound = qfalse;
        S_StartLocalSound( "misc/menu1.wav" );
    }

#if USE_REF == REF_SOFT
    R_SetClipRect( DRAW_CLIP_DISABLED, NULL );
#else
    R_SetScale( NULL );
#endif
    R_SetColor( DRAW_COLOR_CLEAR, NULL );
}

/*
=================
UI_Keydown
=================
*/
void UI_Keydown( int key ) {
    menuSound_t sound;

    if( !uis.activeMenu ) {
        return;
    }

    sound = Menu_Keydown( uis.activeMenu, key );

    switch( sound ) {
    case QMS_IN:
        S_StartLocalSound( "misc/menu1.wav" );
        break;
    case QMS_MOVE:
        S_StartLocalSound( "misc/menu2.wav" );
        break;
    case QMS_OUT:
        S_StartLocalSound( "misc/menu3.wav" );
        break;
    case QMS_BEEP:
        S_StartLocalSound( "misc/talk1.wav" );
        break;
    default:
        break;
    }

}

/*
=================
UI_CharEvent
=================
*/
void UI_CharEvent( int key ) {
    menuCommon_t *item;
    menuSound_t sound;

    if( !uis.activeMenu ) {
        return;
    }

    if( ( item = Menu_ItemAtCursor( uis.activeMenu ) ) == NULL ||
        ( sound = Menu_CharEvent( item, key ) ) == QMS_NOTHANDLED )
    {
        return;
    }

    switch( sound ) {
    case QMS_IN:
        S_StartLocalSound( "misc/menu1.wav" );
        break;
    case QMS_MOVE:
        S_StartLocalSound( "misc/menu2.wav" );
        break;
    case QMS_OUT:
        S_StartLocalSound( "misc/menu3.wav" );
        break;
    case QMS_BEEP:
        S_StartLocalSound( "misc/talk1.wav" );
        break;
    case QMS_NOTHANDLED:
    default:
        break;
    }

}

static void UI_Menu_g( genctx_t *ctx ) {
    menuFrameWork_t *menu;

    LIST_FOR_EACH( menuFrameWork_t, menu, &ui_menus, entry ) {
        if( !Prompt_AddMatch( ctx, menu->name ) ) {
            break;
        }
    }
}

static void UI_PushMenu_c( genctx_t *ctx, int argnum ) {
    if( argnum == 1 ) {
        UI_Menu_g( ctx );
    }
}

static void UI_PushMenu_f( void ) {
    menuFrameWork_t *menu;
    char *s;

    if( Cmd_Argc() < 2 ) {
        Com_Printf( "Usage: %s <menu>\n", Cmd_Argv( 0 ) );
        return;
    }
    s = Cmd_Argv( 1 );
    menu = UI_FindMenu( s );
    if( menu ) {
        UI_PushMenu( menu );
    } else {
        Com_Printf( "No such menu: %s\n", s );
    }
}

static void UI_PopMenu_f( void ) {
    if( uis.activeMenu ) {
        UI_PopMenu();
    }
}


static const cmdreg_t c_ui[] = {
    { "forcemenuoff", UI_ForceMenuOff },
    { "pushmenu", UI_PushMenu_f, UI_PushMenu_c },
    { "popmenu", UI_PopMenu_f },

    { NULL, NULL }
};

static void ui_scale_changed( cvar_t *self ) {
    UI_Resize();
}

void UI_ModeChanged( void ) {
    ui_scale = Cvar_Get( "ui_scale", "1", 0 );
    ui_scale->changed = ui_scale_changed;
    R_GetConfig( &uis.glconfig );
    UI_Resize();
}

static void UI_FreeMenus( void ) {
    menuFrameWork_t *menu, *next;

    LIST_FOR_EACH_SAFE( menuFrameWork_t, menu, next, &ui_menus, entry ) {
        if( menu->free ) {
            menu->free( menu );
        }
    }
    List_Init( &ui_menus );
}


/*
=================
UI_Init
=================
*/
qboolean UI_Init( void ) {
    Cmd_Register( c_ui );

    ui_debug = Cvar_Get( "ui_debug", "0", 0 );
    ui_open = Cvar_Get( "ui_open", "0", CVAR_ARCHIVE );

    UI_ModeChanged();

    uis.fontHandle = R_RegisterFont( "conchars" );
    uis.cursorHandle = R_RegisterPic( "ch1" );
    R_GetPicSize( &uis.cursorWidth, &uis.cursorHeight, uis.cursorHandle );

    Vector4Set( uis.color.background, 0, 0, 0, 255 );
    Vector4Set( uis.color.normal, 15, 128, 235, 100 );
    Vector4Set( uis.color.active, 15, 128, 235, 100 );
    Vector4Set( uis.color.selection, 15, 128, 235, 100 );
    Vector4Set( uis.color.disabled, 127, 127, 127, 255 );

    // load custom menus
    UI_LoadStript();

    // load built-in menus
    M_Menu_PlayerConfig();
    M_Menu_Servers();
    M_Menu_Demos();

    Com_Printf( "Registered %d menus.\n", List_Count( &ui_menus ) );

    uis.initialized = qtrue;

    return qtrue;
}

/*
=================
UI_Shutdown
=================
*/
void UI_Shutdown( void ) {
    if( !uis.initialized ) {
        return;
    }
    UI_ForceMenuOff();

    ui_scale->changed = NULL;

    PlayerModel_Free();

    UI_FreeMenus();

    Cmd_Deregister( c_ui );

    memset( &uis, 0, sizeof( uis ) );

    Z_LeakTest( TAG_UI );
}

