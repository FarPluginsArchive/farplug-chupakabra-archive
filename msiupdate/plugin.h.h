#define PLUGIN_NAME L"<(NAME)>"
#define PLUGIN_VERSION MAKEFARVERSION(<(VER_MAJOR)>, <(VER_MINOR)>, <(VER_PATCH)>, 0, VS_RELEASE)
#ifdef x64
#define PLUGIN_DESCRIPTION L"<(NAME)> for Far Manager <(FAR_VER_MAJOR)> x64"
#else
#define PLUGIN_DESCRIPTION L"<(NAME)> for Far Manager <(FAR_VER_MAJOR)>"
#endif
#define PLUGIN_AUTHOR L"<(AUTHOR)>"
