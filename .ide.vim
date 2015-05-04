"
" Copyright 2015 Dario Manesku. All rights reserved.
" License: http://www.opensource.org/licenses/BSD-2-Clause
"

" Run vim as 'vim --servername VIM'.

if has("unix")
    set makeprg=make

    let s:proj_root    = expand("<sfile>:p:h")
    let s:makebg_file  = s:proj_root."/.makebg.sh"
    let s:log_file     = s:proj_root."/make.log"

    let s:exec_action  = "!../_build/linux64_gcc/bin/cmftStudioDebug"
    let s:gdb_action   = "!gdb -x ".s:proj_root."/.gdbinit"." --args ../_build/linux64_gcc/bin/cmftStudioDebug"

    let s:make_action  = "linux-debug64"

    function! SetDebug()
        let s:make_action = "linux-debug64"
        let s:exec_action = "!../_build/linux64_gcc/bin/cmftStudioDebug"
    endfunc
    command! -nargs=0 SetDebug :call SetDebug()

    function! SetRelease()
        let s:make_action = "linux-release64"
        let s:exec_action = "!../_build/linux64_gcc/bin/cmftStudioRelease"
    endfunc
    command! -nargs=0 SetRelease :call SetRelease()

    function! Build()
        let s:make_command = "make ".s:proj_root." ".s:make_action
        let s:build_action = "!".s:makebg_file." ".v:servername." \"".s:make_command."\" ".s:log_file
        let curr_dir = getcwd()
        exec 'cd' s:proj_root
        exec s:build_action
        exec 'cd' curr_dir
    endfunc

    function! Execute()
        let s:runtime_dir = s:proj_root."/runtime"
        let curr_dir = getcwd()
        exec 'cd' s:runtime_dir
        exec s:exec_action
        exec 'cd' curr_dir
    endfunc

    function! DebugGdb()
        let s:runtime_dir = s:proj_root."/runtime"
        let curr_dir = getcwd()
        exec 'cd' s:runtime_dir
        exec s:gdb_action
        exec 'cd' curr_dir
    endfunc

    nmap ,rr  :call Build()<cr><cr>
    nmap ,ee  :call Execute()<cr>
    nmap ,gdb :call DebugGdb()<cr>

endif
