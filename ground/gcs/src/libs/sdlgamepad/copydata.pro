win32 {
    # copy SDL DLL
    SDL_DLLS = \
        SDL.dll

    # MSYS2 ships sdl12-compat: SDL.dll loads SDL2.dll at run time, which
    # (sdl2-compat) loads SDL3.dll. ntldd cannot see these, so copy them too.
    exists($$[QT_INSTALL_BINS]/SDL2.dll):SDL_DLLS += SDL2.dll
    exists($$[QT_INSTALL_BINS]/SDL3.dll):SDL_DLLS += SDL3.dll

    for(dll, SDL_DLLS) {
        addCopyFileTarget($${dll},$$[QT_INSTALL_BINS],$${GCS_APP_PATH})
        addCopyDependenciesTarget($${dll},$$[QT_INSTALL_BINS],$${GCS_APP_PATH})
    }
}
