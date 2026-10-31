#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>

static int showError(const wchar_t* message) {
    MessageBoxW(nullptr, message, L"Moteur 3D V13.16", MB_OK | MB_ICONERROR);
    return 1;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(101), RT_RCDATA);
    if (!resource) return showError(L"La ressource HTML intégrée est introuvable.");

    HGLOBAL loaded = LoadResource(nullptr, resource);
    if (!loaded) return showError(L"Impossible de charger la page du moteur.");
    const DWORD size = SizeofResource(nullptr, resource);
    const void* bytes = LockResource(loaded);
    if (!bytes || size == 0) return showError(L"La page intégrée est vide.");

    wchar_t tempPath[MAX_PATH]{};
    const DWORD tempLength = GetTempPathW(MAX_PATH, tempPath);
    if (tempLength == 0 || tempLength >= MAX_PATH)
        return showError(L"Impossible de déterminer le dossier temporaire Windows.");

    const std::wstring folder = std::wstring(tempPath) + L"Moteur3D-V13.16";
    if (!CreateDirectoryW(folder.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        return showError(L"Impossible de préparer le dossier temporaire.");

    const std::wstring htmlPath = folder + L"\\moteur3d_v1316_cpu_workers_2026.html";
    HANDLE file = CreateFileW(htmlPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return showError(L"Impossible d’extraire la page HTML temporaire.");

    DWORD written = 0;
    const BOOL ok = WriteFile(file, bytes, size, &written, nullptr);
    CloseHandle(file);
    if (!ok || written != size) {
        DeleteFileW(htmlPath.c_str());
        return showError(L"Échec de l’écriture de la page intégrée.");
    }

    const HINSTANCE opened = ShellExecuteW(nullptr, L"open", htmlPath.c_str(),
                                            nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(opened) <= 32)
        return showError(L"Aucun navigateur par défaut n’a pu ouvrir le moteur.");

    return 0;
}
