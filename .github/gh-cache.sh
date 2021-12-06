#!/usr/bin/env bash
lib_dir=src/lib/public
lib_ext=.lib
dll_ext=.dll
dbg_ext=.pdb
if [ 'uname' = "Linux" ]; then
    lib_dir=src/lib/public/linux32
    lib_ext=.a
    dll_ext=.so
    dbg_ext=.dbg
fi

if [ "$1" != "restore" ]; then
    mkdir -p src/ccache
    mv -f game/tf2vintage/bin/* src/ccache
    mv -f $lib_dir/mathlib$lib_ext $lib_dir/tier1$lib_ext $lib_dir/raytrace$lib_ext $lib_dir/vgui_controls$lib_ext src/ccache
else
    mv -f src/ccache/*$dll_ext src/ccache/*$dbg_ext game/tf2vintage/bin
    mv -f src/ccache/*$lib_ext $lib_dir
fi