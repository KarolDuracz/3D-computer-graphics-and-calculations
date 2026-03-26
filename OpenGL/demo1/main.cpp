#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <GL/gl.h>
#include <GL/glu.h>

#undef min
#undef max

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <io.h>
#include <fcntl.h>
#include <cstdint>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

// ------------------------------------------------------------
// Forward declarations and early globals
// ------------------------------------------------------------
struct Mesh;
struct SceneObject;
struct TextureImage;

static HWND g_hWnd = NULL;

static std::wstring GetDirectoryFromPath(const std::wstring& path);
static std::wstring JoinPathW(const std::wstring& dir, const std::wstring& file);
static std::wstring GetExtensionLowerW(const std::wstring& path);
static std::wstring GetFileNameNoExt(const std::wstring& path);
static std::wstring MakeTemplateBaseName(const std::wstring& modelPath);

static std::wstring SanitizeFileNamePart(const std::wstring& name);
static std::wstring SanitizeFileName(std::wstring s);

static bool EnsureParentDirectoryExists(const std::wstring& filePath);
static void SyncModelControlSelection();

static std::wstring Pad2W(int v);
static bool LoadBMPImageFromFile(const std::wstring& path, TextureImage& out, std::string& errorOut);
static bool SaveBMPImageToFile(const std::wstring& path, const TextureImage& img, std::string& errorOut);
static GLuint CreateGLTextureFromImage(const TextureImage& img);
static bool GenerateTextureTemplateFromMesh(const Mesh& mesh, TextureImage& atlasOut, int& pageSizeOut, int& pageColsOut, int& pageRowsOut, std::string& errorOut);
static bool ComposeTextureAtlasFromManifest(const std::wstring& manifestPath, TextureImage& atlasOut, std::string& errorOut);
static SceneObject* SelectedObject();
static void SelectObject(int index);
static void UpdateWindowTitle();
static void LogSelectedObjectBinding(const char* tag);
static void ReleaseSceneTexture(SceneObject& obj);
static bool ApplyTextureToSceneObjectFromImagePath(SceneObject& obj, const std::wstring& imagePath, const char* sourceTag);
static bool TryAutoLoadSiblingMTL(SceneObject& obj);
static void OpenTextureDialogAndApplyToSelected(bool loadFSH);
static void OpenCreateTextureDialogAndGenerateTemplate();
static void OpenPaintedTextureDialogAndApplyToSelected();
static void HandleMenuLightToggle(UINT cmd);
static void HandlePanelCommand(int cmd);

// ------------------------------------------------------------
// Logging
// ------------------------------------------------------------
static FILE* g_logFile = NULL;
static bool  g_consoleAllocated = false;

static void InitLogging()
{
    if (!g_consoleAllocated) {
        if (AllocConsole()) {
            g_consoleAllocated = true;
            FILE* fp = NULL;
            freopen_s(&fp, "CONOUT$", "w", stdout);
            freopen_s(&fp, "CONOUT$", "w", stderr);
            freopen_s(&fp, "CONIN$", "r", stdin);
        }
    }

    if (!g_logFile) {
        fopen_s(&g_logFile, "obj_scene_demo.log", "w");
    }
}

static void CloseLogging()
{
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = NULL;
    }
}

static void Logf(const char* fmt, ...)
{
    char message[4096];
    char finalMsg[4608];

    va_list args;
    va_start(args, fmt);
#if defined(_MSC_VER)
    vsnprintf_s(message, sizeof(message), _TRUNCATE, fmt, args);
#else
    vsnprintf(message, sizeof(message), fmt, args);
#endif
    va_end(args);

    SYSTEMTIME st;
    GetLocalTime(&st);
#if defined(_MSC_VER)
    sprintf_s(finalMsg, "%04d-%02d-%02d %02d:%02d:%02d.%03d | %s\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, message);
#else
    snprintf(finalMsg, sizeof(finalMsg), "%04d-%02d-%02d %02d:%02d:%02d.%03d | %s\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, message);
#endif

    OutputDebugStringA(finalMsg);
    if (g_logFile) {
        fputs(finalMsg, g_logFile);
        fflush(g_logFile);
    }
    if (g_consoleAllocated) {
        fputs(finalMsg, stdout);
        fflush(stdout);
    }
}

static std::string LastErrorToString(DWORD err)
{
    char* sysMsg = NULL;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD len = FormatMessageA(flags, NULL, err, 0, (LPSTR)&sysMsg, 0, NULL);
    std::string out;
    if (len && sysMsg) {
        out.assign(sysMsg, sysMsg + strlen(sysMsg));
        LocalFree(sysMsg);
    }
    else {
#if defined(_MSC_VER)
        char tmp[64];
        sprintf_s(tmp, "Unknown error %lu", (unsigned long)err);
        out = tmp;
#else
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "Unknown error %lu", (unsigned long)err);
        out = tmp;
#endif
    }
    while (!out.empty() && (out.back() == '\r' || out.back() == '\n' || out.back() == ' ' || out.back() == '\t')) {
        out.pop_back();
    }
    return out;
}

static void LogLastError(const char* where)
{
    DWORD err = GetLastError();
    Logf("ERROR in %s: GetLastError=%lu (%s)", where, (unsigned long)err, LastErrorToString(err).c_str());
}

static void LogOpenGLState(const char* where)
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        Logf("OpenGL error after %s: 0x%04X", where, (unsigned)err);
    }
}

// ------------------------------------------------------------
// Math helpers
// ------------------------------------------------------------
struct Vec2 {
    float x, y;
    Vec2() : x(0), y(0) {}
    Vec2(float _x, float _y) : x(_x), y(_y) {}
};

struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

static Vec3 operator+(const Vec3& a, const Vec3& b) { return Vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
static Vec3 operator-(const Vec3& a, const Vec3& b) { return Vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
static Vec3 operator*(const Vec3& a, float s) { return Vec3(a.x * s, a.y * s, a.z * s); }
static Vec3 operator/(const Vec3& a, float s) { return Vec3(a.x / s, a.y / s, a.z / s); }

static float Dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 Cross(const Vec3& a, const Vec3& b)
{
    return Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

static float Length(const Vec3& v)
{
    return std::sqrt(Dot(v, v));
}

static Vec3 Normalize(const Vec3& v)
{
    float len = Length(v);
    if (len <= 1e-8f) return Vec3(0, 0, 0);
    return v / len;
}

// ------------------------------------------------------------
// OBJ mesh
// ------------------------------------------------------------
struct Vertex {
    Vec3 pos;
    Vec3 normal;
    Vec2 uv;
};

struct Mesh {
    std::vector<Vertex> vertices;
    Vec3 boundsMin;
    Vec3 boundsMax;
    std::string sourcePath;

    Mesh()
        : boundsMin(1e30f, 1e30f, 1e30f),
        boundsMax(-1e30f, -1e30f, -1e30f) {}
};

struct FaceRef {
    int v, vt, vn;
    FaceRef() : v(0), vt(0), vn(0) {}
};

static bool ParseFaceRef(const std::string& token, FaceRef& out)
{
    out = FaceRef();
    int parts[3] = { 0, 0, 0 };
    int partIdx = 0;

    size_t start = 0;
    while (start <= token.size() && partIdx < 3) {
        size_t slash = token.find('/', start);
        std::string item = token.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (!item.empty()) parts[partIdx] = std::atoi(item.c_str());
        partIdx++;
        if (slash == std::string::npos) break;
        start = slash + 1;
        if (start == token.size()) {
            partIdx++;
            break;
        }
    }

    out.v = parts[0];
    out.vt = parts[1];
    out.vn = parts[2];
    return out.v != 0;
}

static int ResolveIndex(int idx, int count)
{
    if (idx > 0) return idx - 1;
    if (idx < 0) return count + idx;
    return -1;
}

static bool LoadOBJFromFile(const wchar_t* filename, Mesh& mesh, std::string& errorOut)
{
    mesh = Mesh();
    mesh.sourcePath = "";
    errorOut.clear();

    Logf("OBJ load: opening '%S'", filename ? filename : L"(null)");
    FILE* fp = NULL;
#if defined(_MSC_VER)
    errno_t ferr = _wfopen_s(&fp, filename, L"rb");
    if (ferr != 0 || !fp) {
        errorOut = "failed to open file";
        Logf("OBJ load failed: _wfopen_s returned %d", (int)ferr);
        return false;
    }
#else
    fp = _wfopen(filename, L"rb");
    if (!fp) {
        errorOut = "failed to open file";
        LogLastError("_wfopen");
        return false;
    }
#endif

    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> texcoords;

    char lineBuf[4096];
    long lineNo = 0;
    size_t faceCount = 0;

    while (fgets(lineBuf, (int)sizeof(lineBuf), fp)) {
        ++lineNo;
        std::string line(lineBuf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        if (line.empty()) continue;
        if (line[0] == '#') continue;

        if (lineNo == 1 && line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) {
            line.erase(0, 3);
        }

        std::istringstream iss(line);
        std::string tag;
        iss >> tag;
        if (tag.empty()) continue;

        if (tag == "v") {
            float x, y, z;
            if (!(iss >> x >> y >> z)) {
                Logf("OBJ parse warning line %ld: bad vertex: %s", lineNo, line.c_str());
                continue;
            }
            positions.push_back(Vec3(x, y, z));
            mesh.boundsMin.x = std::min(mesh.boundsMin.x, x);
            mesh.boundsMin.y = std::min(mesh.boundsMin.y, y);
            mesh.boundsMin.z = std::min(mesh.boundsMin.z, z);
            mesh.boundsMax.x = std::max(mesh.boundsMax.x, x);
            mesh.boundsMax.y = std::max(mesh.boundsMax.y, y);
            mesh.boundsMax.z = std::max(mesh.boundsMax.z, z);
        }
        else if (tag == "vn") {
            float x, y, z;
            if (!(iss >> x >> y >> z)) {
                Logf("OBJ parse warning line %ld: bad normal: %s", lineNo, line.c_str());
                continue;
            }
            normals.push_back(Normalize(Vec3(x, y, z)));
        }
        else if (tag == "vt") {
            float u = 0.0f, v = 0.0f;
            if (!(iss >> u >> v)) {
                Logf("OBJ parse warning line %ld: bad texcoord: %s", lineNo, line.c_str());
                continue;
            }
            texcoords.push_back(Vec2(u, v));
        }
        else if (tag == "f") {
            std::vector<FaceRef> refs;
            std::string tok;
            while (iss >> tok) {
                FaceRef r;
                if (ParseFaceRef(tok, r)) refs.push_back(r);
                else Logf("OBJ parse warning line %ld: bad face token '%s'", lineNo, tok.c_str());
            }

            if (refs.size() < 3) {
                Logf("OBJ parse warning line %ld: face has < 3 vertices", lineNo);
                continue;
            }

            for (size_t i = 1; i + 1 < refs.size(); ++i) {
                FaceRef tri[3] = { refs[0], refs[i], refs[i + 1] };
                Vec3 p[3];
                Vec3 n[3];
                Vec2 t[3];
                bool hasNormal[3] = { false, false, false };

                for (int k = 0; k < 3; ++k) {
                    int pi = ResolveIndex(tri[k].v, (int)positions.size());
                    int ni = ResolveIndex(tri[k].vn, (int)normals.size());
                    int ti = ResolveIndex(tri[k].vt, (int)texcoords.size());

                    if (pi >= 0 && pi < (int)positions.size()) {
                        p[k] = positions[pi];
                    }
                    else {
                        p[k] = Vec3();
                        Logf("OBJ parse warning line %ld: vertex index out of range", lineNo);
                    }

                    if (ni >= 0 && ni < (int)normals.size()) {
                        n[k] = normals[ni];
                        hasNormal[k] = true;
                    }
                    else {
                        n[k] = Vec3(0, 0, 0);
                    }

                    if (ti >= 0 && ti < (int)texcoords.size()) {
                        t[k] = texcoords[ti];
                    }
                    else {
                        t[k] = Vec2();
                    }
                }

                Vec3 faceNormal = Normalize(Cross(p[1] - p[0], p[2] - p[0]));
                for (int k = 0; k < 3; ++k) {
                    Vertex vtx;
                    vtx.pos = p[k];
                    vtx.normal = hasNormal[k] ? n[k] : faceNormal;
                    vtx.uv = t[k];
                    mesh.vertices.push_back(vtx);
                }
                ++faceCount;
            }
        }
    }

    fclose(fp);

    if (positions.empty()) {
        errorOut = "no vertices found";
        Logf("OBJ load failed: no positions parsed");
        return false;
    }

    if (mesh.vertices.empty()) {
        errorOut = "no faces found";
        Logf("OBJ load failed: vertices parsed=%u but no triangulated faces", (unsigned)positions.size());
        return false;
    }

    if (mesh.boundsMin.x > mesh.boundsMax.x) {
        mesh.boundsMin = Vec3(-1, -1, -1);
        mesh.boundsMax = Vec3(1, 1, 1);
    }

#if defined(_MSC_VER)
    wchar_t wpath[MAX_PATH] = { 0 };
    if (filename) {
        wcsncpy_s(wpath, filename, _TRUNCATE);
        char pathA[MAX_PATH * 4] = { 0 };
        WideCharToMultiByte(CP_UTF8, 0, wpath, -1, pathA, (int)sizeof(pathA), NULL, NULL);
        mesh.sourcePath = pathA;
    }
#endif

    Logf("OBJ load success: positions=%u normals=%u uvs=%u triangles=%u vertices=%u",
        (unsigned)positions.size(), (unsigned)normals.size(), (unsigned)texcoords.size(),
        (unsigned)faceCount, (unsigned)mesh.vertices.size());
    Logf("OBJ bounds: min(%.4f, %.4f, %.4f) max(%.4f, %.4f, %.4f)",
        mesh.boundsMin.x, mesh.boundsMin.y, mesh.boundsMin.z,
        mesh.boundsMax.x, mesh.boundsMax.y, mesh.boundsMax.z);

    return true;
}

static Vec3 MeshCenter(const Mesh& mesh)
{
    return (mesh.boundsMin + mesh.boundsMax) * 0.5f;
}

static float MeshScaleToUnit(const Mesh& mesh)
{
    Vec3 ext = mesh.boundsMax - mesh.boundsMin;
    float m = std::max(ext.x, std::max(ext.y, ext.z));
    if (m < 1e-6f) return 1.0f;
    return 2.0f / m;
}

// ------------------------------------------------------------
// Texture and light helpers
// ------------------------------------------------------------
struct TextureImage {
    int width;
    int height;
    std::vector<unsigned char> rgba;

    TextureImage() : width(0), height(0) {}
};

struct LightState {
    bool enabled;
    Vec3 position;
    Vec3 color;
    float intensity;

    LightState()
        : enabled(true), position(2.0f, 4.0f, 3.0f), color(1.0f, 1.0f, 1.0f), intensity(1.0f) {}
};

static LightState g_ambientLight;
static LightState g_directionalLight;

static std::wstring GetDirectoryFromPath(const std::wstring& path)
{
    size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return std::wstring();
    return path.substr(0, slash);
}

static std::wstring JoinPathW(const std::wstring& dir, const std::wstring& file)
{
    if (dir.empty()) return file;
    if (file.empty()) return dir;
    wchar_t last = dir.back();
    if (last == L'\\' || last == L'/') return dir + file;
    return dir + L'\\' + file;
}

static std::wstring GetExtensionLowerW(const std::wstring& path)
{
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return std::wstring();
    std::wstring ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) { return (wchar_t)std::towlower(c); });
    return ext;
}

static std::wstring GetFileNameNoExt(const std::wstring& path)
{
    size_t slash = path.find_last_of(L"\/");
    std::wstring base = (slash == std::wstring::npos) ? path : path.substr(slash + 1);
    size_t dot = base.find_last_of(L'.');
    if (dot == std::wstring::npos) return base;
    return base.substr(0, dot);
}

static std::wstring ChangeExtensionW(const std::wstring& path, const std::wstring& ext)
{
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return path + ext;
    return path.substr(0, dot) + ext;
}

static std::string ToLowerASCII(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static bool EndsWithI(const std::string& s, const std::string& suffix)
{
    if (s.size() < suffix.size()) return false;
    std::string a = ToLowerASCII(s.substr(s.size() - suffix.size()));
    std::string b = ToLowerASCII(suffix);
    return a == b;
}

static std::wstring Utf8ToWString(const std::string& s)
{
    if (s.empty()) return std::wstring();
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    if (needed <= 0) return std::wstring();
    std::wstring out;
    out.resize((size_t)needed - 1);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], needed);
    return out;
}

static bool ReadWholeFileBytes(const std::wstring& path, std::vector<unsigned char>& bytes, std::string& errorOut)
{
    bytes.clear();
    FILE* fp = NULL;
#if defined(_MSC_VER)
    errno_t ferr = _wfopen_s(&fp, path.c_str(), L"rb");
    if (ferr != 0 || !fp) {
        errorOut = "failed to open file";
        return false;
    }
#else
    fp = _wfopen(path.c_str(), L"rb");
    if (!fp) {
        errorOut = "failed to open file";
        return false;
    }
#endif
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        errorOut = "failed to seek file";
        return false;
    }
    long sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        errorOut = "failed to measure file";
        return false;
    }
    rewind(fp);
    bytes.resize((size_t)sz);
    if (sz > 0) {
        size_t got = fread(bytes.data(), 1, (size_t)sz, fp);
        if (got != (size_t)sz) {
            fclose(fp);
            errorOut = "failed to read entire file";
            bytes.clear();
            return false;
        }
    }
    fclose(fp);
    return true;
}

static bool LoadBMPImageFromFile(const std::wstring& path, TextureImage& out, std::string& errorOut)
{
    out = TextureImage();
    std::vector<unsigned char> bytes;
    if (!ReadWholeFileBytes(path, bytes, errorOut)) {
        Logf("Texture load failed: '%S' (%s)", path.c_str(), errorOut.c_str());
        return false;
    }

    if (bytes.size() < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)) {
        errorOut = "file too small for BMP";
        Logf("Texture load failed: '%S' (%s)", path.c_str(), errorOut.c_str());
        return false;
    }

    BITMAPFILEHEADER bfh;
    BITMAPINFOHEADER bih;
    memcpy(&bfh, bytes.data(), sizeof(bfh));
    memcpy(&bih, bytes.data() + sizeof(bfh), sizeof(bih));

    if (bfh.bfType != 0x4D42) {
        errorOut = "not a BMP file";
        Logf("Texture load failed: '%S' (%s)", path.c_str(), errorOut.c_str());
        return false;
    }
    if (bih.biCompression != BI_RGB) {
        errorOut = "compressed BMP not supported";
        Logf("Texture load failed: '%S' (%s)", path.c_str(), errorOut.c_str());
        return false;
    }
    if (bih.biWidth <= 0 || bih.biHeight == 0) {
        errorOut = "invalid BMP dimensions";
        Logf("Texture load failed: '%S' (%s)", path.c_str(), errorOut.c_str());
        return false;
    }

    int width = bih.biWidth;
    int height = (bih.biHeight < 0) ? -bih.biHeight : bih.biHeight;
    bool topDown = bih.biHeight < 0;
    int bpp = bih.biBitCount;

    if (!(bpp == 24 || bpp == 32 || bpp == 8)) {
        errorOut = "only 8/24/32-bit BMP supported";
        Logf("Texture load failed: '%S' (%s)", path.c_str(), errorOut.c_str());
        return false;
    }

    out.width = width;
    out.height = height;
    out.rgba.resize((size_t)width * (size_t)height * 4);

    const unsigned char* pixelBase = bytes.data() + bfh.bfOffBits;
    size_t paletteOffset = sizeof(BITMAPFILEHEADER) + bih.biSize;
    const unsigned char* palette = (bpp == 8 && bfh.bfOffBits > paletteOffset && bytes.size() >= paletteOffset) ? bytes.data() + paletteOffset : NULL;

    int rowStride = ((width * bpp + 31) / 32) * 4;
    if (bytes.size() < (size_t)bfh.bfOffBits + (size_t)rowStride * (size_t)height) {
        errorOut = "BMP pixel data truncated";
        Logf("Texture load failed: '%S' (%s)", path.c_str(), errorOut.c_str());
        return false;
    }

    for (int y = 0; y < height; ++y) {
        int srcY = topDown ? y : (height - 1 - y);
        const unsigned char* row = pixelBase + (size_t)srcY * (size_t)rowStride;
        for (int x = 0; x < width; ++x) {
            unsigned char r = 255, g = 255, b = 255, a = 255;
            if (bpp == 24) {
                const unsigned char* px = row + x * 3;
                b = px[0]; g = px[1]; r = px[2];
            }
            else if (bpp == 32) {
                const unsigned char* px = row + x * 4;
                b = px[0]; g = px[1]; r = px[2]; a = px[3];
            }
            else if (bpp == 8) {
                unsigned char idx = row[x];
                if (palette) {
                    const unsigned char* pe = palette + (size_t)idx * 4;
                    b = pe[0]; g = pe[1]; r = pe[2]; a = 255;
                }
                else {
                    r = g = b = idx;
                }
            }
            size_t di = ((size_t)y * (size_t)width + (size_t)x) * 4;
            out.rgba[di + 0] = r;
            out.rgba[di + 1] = g;
            out.rgba[di + 2] = b;
            out.rgba[di + 3] = a;
        }
    }

    Logf("BMP load success: '%S' %dx%d bpp=%d", path.c_str(), width, height, bpp);
    return true;
}

static GLuint CreateGLTextureFromImage(const TextureImage& img)
{
    if (img.width <= 0 || img.height <= 0 || img.rgba.empty()) return 0;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (!tex) {
        Logf("CreateGLTextureFromImage: glGenTextures failed");
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLenum srcFormat = GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width, img.height, 0, srcFormat, GL_UNSIGNED_BYTE, img.rgba.data());
    LogOpenGLState("glTexImage2D");
    return tex;
}

static bool FileExistsW(const std::wstring& path)
{
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static std::wstring ResolveRelativeTo(const std::wstring& baseFile, const std::wstring& candidate)
{
    if (candidate.empty()) return candidate;
    if (candidate.find(L':') != std::wstring::npos || (!candidate.empty() && (candidate[0] == L'\\' || candidate[0] == L'/'))) {
        return candidate;
    }
    std::wstring dir = GetDirectoryFromPath(baseFile);
    return JoinPathW(dir, candidate);
}

static bool ParseMTLTexturePath(const std::wstring& mtlPath, std::wstring& outTexturePath, std::string& errorOut)
{
    outTexturePath.clear();
    std::wstring resolvedPath;
    FILE* fp = NULL;
    Logf("MTL parse start: '%S'", mtlPath.c_str());
#if defined(_MSC_VER)
    errno_t ferr = _wfopen_s(&fp, mtlPath.c_str(), L"rb");
    if (ferr != 0 || !fp) {
        errorOut = "failed to open MTL file";
        return false;
    }
#else
    fp = _wfopen(mtlPath.c_str(), L"rb");
    if (!fp) {
        errorOut = "failed to open MTL file";
        return false;
    }
#endif

    char lineBuf[4096];
    long lineNo = 0;
    while (fgets(lineBuf, (int)sizeof(lineBuf), fp)) {
        ++lineNo;
        std::string line(lineBuf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string tag;
        iss >> tag;
        if (tag == "map_Kd" || tag == "map_kd") {
            std::string rest;
            std::getline(iss, rest);
            while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) rest.erase(rest.begin());
            if (!rest.empty() && rest.front() == '"' && rest.back() == '"') {
                rest = rest.substr(1, rest.size() - 2);
            }
            Logf("MTL parse line %ld: map_Kd '%s'", lineNo, rest.c_str());
            resolvedPath = ResolveRelativeTo(mtlPath, Utf8ToWString(rest));
            Logf("MTL parse resolved texture: '%S'", resolvedPath.c_str());
            break;
        }
        else {
            Logf("MTL parse line %ld: tag='%s' ignored", lineNo, tag.c_str());
        }
    }
    fclose(fp);

    if (resolvedPath.empty()) {
        errorOut = "no map_Kd entry found";
        Logf("MTL parse failed: '%S' (%s)", mtlPath.c_str(), errorOut.c_str());
        return false;
    }
    outTexturePath = resolvedPath;
    Logf("MTL parse success: '%S' -> '%S'", mtlPath.c_str(), outTexturePath.c_str());
    return true;
}

static bool ParseFSHTexturePath(const std::wstring& fshPath, std::vector<std::wstring>& allTextures, std::string& errorOut)
{
    allTextures.clear();
    Logf("FSH parse start: '%S'", fshPath.c_str());

    std::vector<unsigned char> bytes;
    if (!ReadWholeFileBytes(fshPath, bytes, errorOut)) {
        return false;
    }

    std::string text;
    text.reserve(bytes.size());
    for (size_t i = 0; i < bytes.size(); ++i) {
        unsigned char c = bytes[i];
        if (c == 0) {
            text.push_back(' ');
        }
        else if (c == '\n' || c == '\r' || c == '\t' || (c >= 32 && c < 127)) {
            text.push_back((char)c);
        }
        else {
            text.push_back(' ');
        }
    }

    std::istringstream iss(text);
    std::string token;
    std::vector<std::string> found;
    while (iss >> token) {
        std::string clean = token;
        while (!clean.empty() && (clean.back() == '\0' || clean.back() == ',' || clean.back() == ';')) clean.pop_back();
        if (EndsWithI(clean, ".bmp")) {
            found.push_back(clean);
        }
    }

    // Fallback: search for any BMP-like substrings in the raw text.
    if (found.empty()) {
        size_t pos = 0;
        while ((pos = ToLowerASCII(text).find(".bmp", pos)) != std::string::npos) {
            size_t start = pos;
            while (start > 0) {
                char ch = text[start - 1];
                if (std::isalnum((unsigned char)ch) || ch == '_' || ch == '-' || ch == '.') --start;
                else break;
            }
            std::string candidate = text.substr(start, pos + 4 - start);
            if (!candidate.empty()) found.push_back(candidate);
            pos += 4;
        }
    }

    std::wstring dir = GetDirectoryFromPath(fshPath);
    for (size_t i = 0; i < found.size(); ++i) {
        std::wstring w = Utf8ToWString(found[i]);
        w = ResolveRelativeTo(fshPath, w);
        if (FileExistsW(w)) {
            allTextures.push_back(w);
            Logf("FSH parser: texture candidate '%S'", w.c_str());
        }
        else {
            // still keep the candidate path for logging, in case the caller wants to look manually
            allTextures.push_back(w);
            Logf("FSH parser: texture candidate not found '%S'", w.c_str());
        }
    }

    if (allTextures.empty()) {
        errorOut = "no BMP references found";
        Logf("FSH parse failed: '%S' (%s)", fshPath.c_str(), errorOut.c_str());
        return false;
    }

    Logf("FSH parse success: '%S' candidateCount=%u", fshPath.c_str(), (unsigned)allTextures.size());
    return true;
}

static void ApplyLighting()
{
    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    GLfloat globalAmbient[] = {
        g_ambientLight.enabled ? (0.18f * g_ambientLight.intensity * g_ambientLight.color.x) : 0.05f,
        g_ambientLight.enabled ? (0.18f * g_ambientLight.intensity * g_ambientLight.color.y) : 0.05f,
        g_ambientLight.enabled ? (0.18f * g_ambientLight.intensity * g_ambientLight.color.z) : 0.05f,
        1.0f
    };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    if (g_directionalLight.enabled) {
        glEnable(GL_LIGHT0);
        GLfloat pos[] = { g_directionalLight.position.x, g_directionalLight.position.y, g_directionalLight.position.z, 0.0f };
        GLfloat diff[] = {
            g_directionalLight.color.x * g_directionalLight.intensity,
            g_directionalLight.color.y * g_directionalLight.intensity,
            g_directionalLight.color.z * g_directionalLight.intensity,
            1.0f
        };
        GLfloat amb[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        GLfloat spec[] = { 0.12f, 0.12f, 0.12f, 1.0f };
        glLightfv(GL_LIGHT0, GL_POSITION, pos);
        glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, diff);
        glLightfv(GL_LIGHT0, GL_SPECULAR, spec);
    }
    else {
        glDisable(GL_LIGHT0);
    }

    // Ambient light is represented by global ambient for the fixed-function pipeline.
}

static void PutImagePixel(TextureImage& img, int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
{
    if (x < 0 || y < 0 || x >= img.width || y >= img.height) return;
    size_t i = ((size_t)y * (size_t)img.width + (size_t)x) * 4;
    img.rgba[i + 0] = r;
    img.rgba[i + 1] = g;
    img.rgba[i + 2] = b;
    img.rgba[i + 3] = a;
}

static void FillImage(TextureImage& img, unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
{
    if (img.width <= 0 || img.height <= 0) return;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            PutImagePixel(img, x, y, r, g, b, a);
        }
    }
}

static void DrawLineImage(TextureImage& img, int x0, int y0, int x1, int y1, unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
{
    int dx = std::abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        PutImagePixel(img, x0, y0, r, g, b, a);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void DrawRectImage(TextureImage& img, int x0, int y0, int x1, int y1, unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
{
    DrawLineImage(img, x0, y0, x1, y0, r, g, b, a);
    DrawLineImage(img, x1, y0, x1, y1, r, g, b, a);
    DrawLineImage(img, x1, y1, x0, y1, r, g, b, a);
    DrawLineImage(img, x0, y1, x0, y0, r, g, b, a);
}

static void DrawCrossImage(TextureImage& img, int cx, int cy, int size, unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
{
    DrawLineImage(img, cx - size, cy, cx + size, cy, r, g, b, a);
    DrawLineImage(img, cx, cy - size, cx, cy + size, r, g, b, a);
}

static int WrapCoordToPixel(float v, int size)
{
    if (size <= 1) return 0;
    float f = v - std::floor(v);
    if (f < 0.0f) f += 1.0f;
    int px = (int)std::floor(f * (float)(size - 1) + 0.5f);
    if (px < 0) px = 0;
    if (px >= size) px = size - 1;
    return px;
}

static bool UVToPixel(const Vec2& uv, int w, int h, int& x, int& y, bool& wrapped)
{
    wrapped = false;
    float u = uv.x;
    float v = uv.y;
    if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) wrapped = true;
    x = WrapCoordToPixel(u, w);
    y = (h > 1) ? (h - 1 - WrapCoordToPixel(v, h)) : 0;
    return true;
}

static void DrawTinyDigit(TextureImage& img, int x, int y, int digit, int scale, unsigned char r, unsigned char g, unsigned char b)
{
    static const unsigned char font[10][5] = {
        { 0x07, 0x05, 0x05, 0x05, 0x07 },
        { 0x02, 0x06, 0x02, 0x02, 0x07 },
        { 0x07, 0x01, 0x07, 0x04, 0x07 },
        { 0x07, 0x01, 0x07, 0x01, 0x07 },
        { 0x05, 0x05, 0x07, 0x01, 0x01 },
        { 0x07, 0x04, 0x07, 0x01, 0x07 },
        { 0x07, 0x04, 0x07, 0x05, 0x07 },
        { 0x07, 0x01, 0x02, 0x04, 0x04 },
        { 0x07, 0x05, 0x07, 0x05, 0x07 },
        { 0x07, 0x05, 0x07, 0x01, 0x07 }
    };
    if (digit < 0 || digit > 9 || scale <= 0) return;
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 3; ++col) {
            if (font[digit][row] & (1 << (2 - col))) {
                for (int sy = 0; sy < scale; ++sy) {
                    for (int sx = 0; sx < scale; ++sx) {
                        PutImagePixel(img, x + col * scale + sx, y + row * scale + sy, r, g, b, 255);
                    }
                }
            }
        }
    }
}

static void DrawTinyNumber(TextureImage& img, int x, int y, int value, int scale, unsigned char r, unsigned char g, unsigned char b)
{
    if (value < 0) {
        PutImagePixel(img, x, y, r, g, b, 255);
        return;
    }
    std::string s = std::to_string(value);
    int cursor = x;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c >= '0' && c <= '9') {
            DrawTinyDigit(img, cursor, y, c - '0', scale, r, g, b);
            cursor += 4 * scale;
        }
        else {
            cursor += 2 * scale;
        }
    }
}

static bool SaveBMPImageToFile(const std::wstring& path, const TextureImage& img, std::string& errorOut)
{
    errorOut.clear();

    if (img.width <= 0 || img.height <= 0 || img.rgba.empty()) {
        errorOut = "invalid image";
        Logf("SaveBMPImageToFile: invalid image size=%dx%d bytes=%u", img.width, img.height, (unsigned)img.rgba.size());
        return false;
    }

    if (!EnsureParentDirectoryExists(path)) {
        errorOut = "failed to create output directory";
        Logf("SaveBMPImageToFile: EnsureParentDirectoryExists failed for '%S'", path.c_str());
        return false;
    }

    // hard code path to model
    const std::wstring new_path = L"C:\\Windows\\Temp\\f1carmodel\\test_p00.bmp";

    HANDLE hFile = CreateFileW(
        //path.c_str(),
        new_path.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        errorOut = "failed to open output BMP";
        Logf("SaveBMPImageToFile: CreateFileW failed for '%S'", path.c_str());
        LogLastError("CreateFileW(output bmp)");
        return false;
    }

    BITMAPFILEHEADER bfh;
    BITMAPINFOHEADER bih;
    ZeroMemory(&bfh, sizeof(bfh));
    ZeroMemory(&bih, sizeof(bih));
    bfh.bfType = 0x4D42;
    bfh.bfOffBits = sizeof(bfh) + sizeof(bih);
    bih.biSize = sizeof(bih);
    bih.biWidth = img.width;
    bih.biHeight = -img.height; // top-down
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = (DWORD)(img.width * img.height * 4);
    bfh.bfSize = bfh.bfOffBits + bih.biSizeImage;

    DWORD written = 0;
    if (!WriteFile(hFile, &bfh, sizeof(bfh), &written, NULL) || written != sizeof(bfh)) {
        errorOut = "failed to write BMP header";
        LogLastError("WriteFile(BITMAPFILEHEADER)");
        CloseHandle(hFile);
        return false;
    }

    if (!WriteFile(hFile, &bih, sizeof(bih), &written, NULL) || written != sizeof(bih)) {
        errorOut = "failed to write BMP header";
        LogLastError("WriteFile(BITMAPINFOHEADER)");
        CloseHandle(hFile);
        return false;
    }

    std::vector<unsigned char> flipped;
    flipped.resize((size_t)img.width * (size_t)img.height * 4);

    for (int y = 0; y < img.height; ++y) {
        const unsigned char* srcRow = &img.rgba[(size_t)(img.height - 1 - y) * (size_t)img.width * 4];
        unsigned char* dstRow = &flipped[(size_t)y * (size_t)img.width * 4];
        for (int x = 0; x < img.width; ++x) {
            const unsigned char* s = srcRow + x * 4;
            unsigned char* d = dstRow + x * 4;
            d[0] = s[2];
            d[1] = s[1];
            d[2] = s[0];
            d[3] = s[3];
        }
    }

    if (!WriteFile(hFile, flipped.data(), (DWORD)flipped.size(), &written, NULL) || written != flipped.size()) {
        errorOut = "failed to write BMP pixels";
        LogLastError("WriteFile(pixel data)");
        CloseHandle(hFile);
        return false;
    }

    CloseHandle(hFile);
    Logf("BMP save success: '%S' %dx%d", path.c_str(), img.width, img.height);
    return true;
}

static std::wstring Pad2W(int v)
{
    wchar_t buf[16];
#if defined(_MSC_VER)
    swprintf_s(buf, L"%02d", v);
#else
    swprintf(buf, 16, L"%02d", v);
#endif
    return buf;
}

// ------------------------------------------------------------
// Scene objects
// ------------------------------------------------------------
struct SceneObject {
    Mesh mesh;
    std::wstring sourcePathW;
    std::wstring displayNameW;
    std::wstring textureSourceW;
    Vec3 position;
    Vec3 rotation;
    Vec3 scale;
    Vec3 meshCenter;
    float meshScale;
    GLuint textureId;
    bool hasTexture;

    SceneObject()
        : position(0, 0, 0), rotation(0, 0, 0), scale(1, 1, 1),
        meshCenter(0, 0, 0), meshScale(1.0f), textureId(0), hasTexture(false) {}
};

static std::vector<SceneObject> g_sceneObjects;
static int g_selectedObject = -1;
static std::wstring g_currentObjPath = L"model.obj";

#if 1
static std::wstring SanitizeFileNamePart(const std::wstring& name)
{
    std::wstring s = name;
    for (wchar_t& ch : s) {
        switch (ch) {
        case L'\\':
        case L'/':
        case L':':
        case L'*':
        case L'?':
        case L'"':
        case L'<':
        case L'>':
        case L'|':
            ch = L'_';
            break;
        default:
            break;
        }
    }
    while (!s.empty() && (s.back() == L' ' || s.back() == L'.')) {
        s.pop_back();
    }
    if (s.empty()) s = L"texture";
    return s;
}
#endif

#if 0
static std::wstring MakeTemplateBaseName(const std::wstring& modelPath)
{
    return SanitizeFileNamePart(GetFileNameNoExt(modelPath)) + L"_tex";
}
#endif

#if 1
static std::wstring SanitizeFileName(std::wstring s)
{
    for (wchar_t& ch : s) {
        switch (ch) {
        case L'\\':
        case L'/':
        case L':':
        case L'*':
        case L'?':
        case L'"':
        case L'<':
        case L'>':
        case L'|':
            ch = L'_';
            break;
        default:
            break;
        }
    }

    while (!s.empty() && (s.back() == L' ' || s.back() == L'.')) {
        s.pop_back();
    }

    if (s.empty()) {
        s = L"texture";
    }

    return s;
}

static bool EnsureParentDirectoryExists(const std::wstring& filePath)
{
    size_t slash = filePath.find_last_of(L"\\/");

    if (slash == std::wstring::npos) {
        return true;
    }

    std::wstring dir = filePath.substr(0, slash);
    if (dir.empty()) {
        return true;
    }

    std::wstring partial;
    partial.reserve(dir.size());

    for (size_t i = 0; i < dir.size(); ++i) {
        wchar_t ch = dir[i];
        partial.push_back(ch);

        if (ch == L'\\' || ch == L'/' || i == dir.size() - 1) {
            if (partial.size() == 3 && partial[1] == L':' &&
                (partial[2] == L'\\' || partial[2] == L'/')) {
                continue;
            }

            if (partial.size() > 0) {
                CreateDirectoryW(partial.c_str(), NULL);
            }
        }
    }

    return true;
}

static bool SaveBitmap32ToFile(const std::wstring& outPath, int width, int height, const std::vector<unsigned char>& rgba)
{
    if (width <= 0 || height <= 0) {
        Logf("SaveBitmap32ToFile: invalid size %dx%d", width, height);
        return false;
    }

    if ((int)rgba.size() < width * height * 4) {
        Logf("SaveBitmap32ToFile: pixel buffer too small (%u bytes, need %u)",
            (unsigned)rgba.size(), (unsigned)(width * height * 4));
        return false;
    }

    EnsureParentDirectoryExists(outPath);

    HANDLE hFile = CreateFileW(
        outPath.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        LogLastError("CreateFileW(output bmp)");
        return false;
    }

    BITMAPFILEHEADER bmf = {};
    BITMAPINFOHEADER bmi = {};

    bmi.biSize = sizeof(BITMAPINFOHEADER);
    bmi.biWidth = width;
    bmi.biHeight = height;
    bmi.biPlanes = 1;
    bmi.biBitCount = 32;
    bmi.biCompression = BI_RGB;
    bmi.biSizeImage = width * height * 4;

    bmf.bfType = 0x4D42;
    bmf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bmf.bfSize = bmf.bfOffBits + bmi.biSizeImage;

    DWORD written = 0;
    if (!WriteFile(hFile, &bmf, sizeof(bmf), &written, NULL) || written != sizeof(bmf)) {
        LogLastError("WriteFile(BITMAPFILEHEADER)");
        CloseHandle(hFile);
        return false;
    }

    if (!WriteFile(hFile, &bmi, sizeof(bmi), &written, NULL) || written != sizeof(bmi)) {
        LogLastError("WriteFile(BITMAPINFOHEADER)");
        CloseHandle(hFile);
        return false;
    }

    std::vector<unsigned char> flipped;
    flipped.resize((size_t)width * (size_t)height * 4);

    for (int y = 0; y < height; ++y) {
        const unsigned char* srcRow = &rgba[(size_t)(height - 1 - y) * (size_t)width * 4];
        unsigned char* dstRow = &flipped[(size_t)y * (size_t)width * 4];
        for (int x = 0; x < width; ++x) {
            const unsigned char* s = srcRow + x * 4;
            unsigned char* d = dstRow + x * 4;
            d[0] = s[2];
            d[1] = s[1];
            d[2] = s[0];
            d[3] = s[3];
        }
    }

    if (!WriteFile(hFile, flipped.data(), (DWORD)flipped.size(), &written, NULL) || written != flipped.size()) {
        LogLastError("WriteFile(pixel data)");
        CloseHandle(hFile);
        return false;
    }

    CloseHandle(hFile);
    return true;
}

static bool ShowSaveTextureDialog(std::wstring& outPath)
{
    wchar_t fileName[MAX_PATH] = { 0 };

    std::wstring baseName = L"texture";
    SceneObject* obj = SelectedObject();
    if (obj && !obj->displayNameW.empty()) {
        baseName = obj->displayNameW;
    }

    size_t dot = baseName.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        baseName = baseName.substr(0, dot);
    }

    baseName = SanitizeFileName(baseName);
    wcsncpy_s(fileName, MAX_PATH, (baseName + L"_template.bmp").c_str(), _TRUNCATE);

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"BMP Files (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"bmp";
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    ofn.lpstrTitle = L"Save generated texture";

    Logf("Create texture: showing save dialog");
    if (GetSaveFileNameW(&ofn)) {
        outPath = fileName;
        Logf("Create texture: save path selected '%S'", fileName);
        return true;
    }

    DWORD err = CommDlgExtendedError();
    if (err != 0) {
        Logf("Create texture dialog error=0x%08lX", (unsigned long)err);
    }
    else {
        Logf("Create texture dialog canceled");
    }
    return false;
}

static bool SaveGeneratedTextureTemplate(const std::wstring& outPath, int width, int height, const std::vector<unsigned char>& rgba)
{
    Logf("Create texture save: '%S' size=%dx%d", outPath.c_str(), width, height);
    if (!SaveBitmap32ToFile(outPath, width, height, rgba)) {
        Logf("Create texture save failed: failed to open output BMP");
        return false;
    }
    Logf("Create texture save success: '%S'", outPath.c_str());
    return true;
}

static std::wstring MakeTemplateBaseName(const std::wstring& modelPath)
{
    return SanitizeFileName(GetFileNameNoExt(modelPath)) + L"_tex";
}
#endif


static void ChooseTemplateAtlasSize(const Mesh& mesh, int& pageSize, int& pageCols, int& pageRows)
{
    size_t faceCount = mesh.vertices.size() / 3;
    if (faceCount <= 1500) pageSize = 1024;
    else if (faceCount <= 6000) pageSize = 2048;
    else pageSize = 4096;

    int pageCount = 1;
    if (faceCount > 2000) {
        pageCount = (int)((faceCount + 1999) / 2000);
    }
    pageCols = 1;
    while (pageCols * pageCols < pageCount) ++pageCols;
    pageRows = (pageCount + pageCols - 1) / pageCols;
    if (pageRows < 1) pageRows = 1;
}

static void LogTemplateStats(const Mesh& mesh, int atlasW, int atlasH, int pageSize, int pageCols, int pageRows)
{
    size_t faceCount = mesh.vertices.size() / 3;
    float minU = 1e30f, minV = 1e30f, maxU = -1e30f, maxV = -1e30f;
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        minU = std::min(minU, mesh.vertices[i].uv.x);
        minV = std::min(minV, mesh.vertices[i].uv.y);
        maxU = std::max(maxU, mesh.vertices[i].uv.x);
        maxV = std::max(maxV, mesh.vertices[i].uv.y);
    }
    if (mesh.vertices.empty()) {
        minU = minV = 0.0f;
        maxU = maxV = 0.0f;
    }
    Logf("Texture template stats: faces=%u atlas=%dx%d tile=%dx%d pages=%dx%d", (unsigned)faceCount, atlasW, atlasH, pageSize, pageSize, pageCols, pageRows);
    Logf("Texture template UV bounds: min(%.4f, %.4f) max(%.4f, %.4f)", minU, minV, maxU, maxV);
}

static bool GenerateTextureTemplateFromMesh(const Mesh& mesh, TextureImage& atlasOut, int& pageSizeOut, int& pageColsOut, int& pageRowsOut, std::string& errorOut)
{
    errorOut.clear();
    if (mesh.vertices.empty()) {
        errorOut = "selected model has no mesh data";
        return false;
    }

    int pageSize = 1024;
    int pageCols = 1;
    int pageRows = 1;
    ChooseTemplateAtlasSize(mesh, pageSize, pageCols, pageRows);
    int atlasW = pageSize * pageCols;
    int atlasH = pageSize * pageRows;

    atlasOut = TextureImage();
    atlasOut.width = atlasW;
    atlasOut.height = atlasH;
    atlasOut.rgba.resize((size_t)atlasW * (size_t)atlasH * 4);
    FillImage(atlasOut, 168, 168, 168, 255);

    for (int y = 0; y < atlasH; ++y) {
        for (int x = 0; x < atlasW; ++x) {
            if ((x % 256) == 0 || (y % 256) == 0) {
                PutImagePixel(atlasOut, x, y, 150, 150, 150, 255);
            }
        }
    }

    size_t faceCount = mesh.vertices.size() / 3;
    size_t wrappedCount = 0;
    size_t degenerateCount = 0;

    for (size_t fi = 0; fi < faceCount; ++fi) {
        const Vertex& a = mesh.vertices[fi * 3 + 0];
        const Vertex& b = mesh.vertices[fi * 3 + 1];
        const Vertex& c = mesh.vertices[fi * 3 + 2];

        int x0, y0, x1, y1, x2, y2;
        bool wrapped = false;
        UVToPixel(a.uv, atlasW, atlasH, x0, y0, wrapped);
        UVToPixel(b.uv, atlasW, atlasH, x1, y1, wrapped);
        UVToPixel(c.uv, atlasW, atlasH, x2, y2, wrapped);
        if (wrapped) ++wrappedCount;

        if ((x0 == x1 && y0 == y1) || (x1 == x2 && y1 == y2) || (x2 == x0 && y2 == y0)) {
            ++degenerateCount;
            continue;
        }

        DrawLineImage(atlasOut, x0, y0, x1, y1, 255, 230, 0, 255);
        DrawLineImage(atlasOut, x1, y1, x2, y2, 255, 230, 0, 255);
        DrawLineImage(atlasOut, x2, y2, x0, y0, 255, 230, 0, 255);

        int cx = (x0 + x1 + x2) / 3;
        int cy = (y0 + y1 + y2) / 3;
        DrawCrossImage(atlasOut, cx, cy, 4, 255, 240, 0, 255);
        DrawTinyNumber(atlasOut, cx + 6, cy + 2, (int)fi, 2, 255, 240, 0);
    }

    // page boundaries and page indices
    for (int py = 0; py < pageRows; ++py) {
        for (int px = 0; px < pageCols; ++px) {
            int x0 = px * pageSize;
            int y0 = py * pageSize;
            int x1 = std::min(atlasW - 1, x0 + pageSize - 1);
            int y1 = std::min(atlasH - 1, y0 + pageSize - 1);
            DrawRectImage(atlasOut, x0, y0, x1, y1, 255, 255, 0, 255);
            int pageIndex = py * pageCols + px;
            DrawTinyNumber(atlasOut, x0 + 6, y0 + 6, pageIndex, 2, 255, 255, 0);
        }
    }

    if (wrappedCount > 0) {
        Logf("Texture template: %u faces had UVs outside 0..1 and were wrapped for preview", (unsigned)wrappedCount);
    }
    if (degenerateCount > 0) {
        Logf("Texture template: %u degenerate faces skipped", (unsigned)degenerateCount);
    }

    pageSizeOut = pageSize;
    pageColsOut = pageCols;
    pageRowsOut = pageRows;
    LogTemplateStats(mesh, atlasW, atlasH, pageSize, pageCols, pageRows);
    Logf("Texture template generated: atlas=%dx%d pages=%dx%d tile=%d faces=%u", atlasW, atlasH, pageCols, pageRows, pageSize, (unsigned)faceCount);
    return true;
}

struct TemplatePackInfo {
    int atlasW;
    int atlasH;
    int pageSize;
    int cols;
    int rows;
    std::vector<std::wstring> pages;
    TemplatePackInfo() : atlasW(0), atlasH(0), pageSize(0), cols(0), rows(0) {}
};

static bool ParseTemplateManifest(const std::wstring& manifestPath, TemplatePackInfo& out, std::string& errorOut)
{
    out = TemplatePackInfo();
    errorOut.clear();

    std::vector<unsigned char> bytes;
    if (!ReadWholeFileBytes(manifestPath, bytes, errorOut)) {
        return false;
    }
    std::string text(bytes.begin(), bytes.end());
    std::istringstream iss(text);
    std::string line;
    long lineNo = 0;
    while (std::getline(iss, line)) {
        ++lineNo;
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        if (line == "TEMPLATEPACK v1") continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        if (key == "atlas_w") out.atlasW = std::atoi(value.c_str());
        else if (key == "atlas_h") out.atlasH = std::atoi(value.c_str());
        else if (key == "page_size") out.pageSize = std::atoi(value.c_str());
        else if (key == "cols") out.cols = std::atoi(value.c_str());
        else if (key == "rows") out.rows = std::atoi(value.c_str());
        else if (key.find("page_") == 0) out.pages.push_back(ResolveRelativeTo(manifestPath, Utf8ToWString(value)));
    }

    if (out.atlasW <= 0 || out.atlasH <= 0 || out.pageSize <= 0 || out.cols <= 0 || out.rows <= 0 || out.pages.empty()) {
        errorOut = "invalid or incomplete manifest";
        return false;
    }

    Logf("Texture manifest parsed: atlas=%dx%d tile=%d cols=%d rows=%d pages=%u", out.atlasW, out.atlasH, out.pageSize, out.cols, out.rows, (unsigned)out.pages.size());
    return true;
}

static bool ComposeTextureAtlasFromManifest(const std::wstring& manifestPath, TextureImage& atlasOut, std::string& errorOut)
{

    // hard code manifestPath 
    const std::wstring manifestPath1 = L"C:\\Windows\\Temp\\f1carmodel\\test_p00.bmp";

    TemplatePackInfo info;
    if (!ParseTemplateManifest(manifestPath1, info, errorOut)) {
        Logf("Texture manifest parse failed: '%S' (%s)", manifestPath.c_str(), errorOut.c_str());
        return false;
    }

        // oryginalny kod
#if 0
    TemplatePackInfo info;
    if (!ParseTemplateManifest(manifestPath, info, errorOut)) {
        Logf("Texture manifest parse failed: '%S' (%s)", manifestPath.c_str(), errorOut.c_str());
        return false;
    }
#endif

    atlasOut = TextureImage();
    atlasOut.width = info.atlasW;
    atlasOut.height = info.atlasH;
    atlasOut.rgba.resize((size_t)info.atlasW * (size_t)info.atlasH * 4);
    FillImage(atlasOut, 0, 0, 0, 255);

    if ((int)info.pages.size() < info.cols * info.rows) {
        Logf("Texture manifest note: pages=%u but grid expects=%d", (unsigned)info.pages.size(), info.cols * info.rows);
    }

    for (size_t i = 0; i < info.pages.size(); ++i) {
        const std::wstring& pagePath = info.pages[i];
        TextureImage pageImg;
        std::string pageErr;
        if (!LoadBMPImageFromFile(pagePath, pageImg, pageErr)) {
            errorOut = pageErr;
            Logf("Texture page load failed: '%S' (%s)", pagePath.c_str(), pageErr.c_str());
            return false;
        }
        if (pageImg.width != info.pageSize || pageImg.height != info.pageSize) {
            Logf("Texture page size mismatch: '%S' got %dx%d expected %dx%d", pagePath.c_str(), pageImg.width, pageImg.height, info.pageSize, info.pageSize);
        }

        int px = (int)(i % (size_t)info.cols) * info.pageSize;
        int py = (int)(i / (size_t)info.cols) * info.pageSize;
        for (int y = 0; y < pageImg.height && py + y < atlasOut.height; ++y) {
            for (int x = 0; x < pageImg.width && px + x < atlasOut.width; ++x) {
                size_t si = ((size_t)y * (size_t)pageImg.width + (size_t)x) * 4;
                size_t di = ((size_t)(py + y) * (size_t)atlasOut.width + (size_t)(px + x)) * 4;
                atlasOut.rgba[di + 0] = pageImg.rgba[si + 0];
                atlasOut.rgba[di + 1] = pageImg.rgba[si + 1];
                atlasOut.rgba[di + 2] = pageImg.rgba[si + 2];
                atlasOut.rgba[di + 3] = pageImg.rgba[si + 3];
            }
        }
        Logf("Texture page blitted: '%S' -> tile(%d,%d)", pagePath.c_str(), px / info.pageSize, py / info.pageSize);
    }

    Logf("Texture atlas composed from manifest '%S'", manifestPath.c_str());
    return true;
}

static bool SaveTextureTemplatePack(const std::wstring& outputManifestPath, const TextureImage& atlas, int pageSize, int pageCols, int pageRows, std::string& errorOut)
{
    errorOut.clear();
    std::wstring baseDir = GetDirectoryFromPath(outputManifestPath);
    std::wstring baseName = GetFileNameNoExt(outputManifestPath);
    if (baseName.empty()) baseName = L"texture_template";
    if (pageSize <= 0 || pageCols <= 0 || pageRows <= 0) {
        errorOut = "invalid pack layout";
        return false;
    }

    if (!baseDir.empty()) {
        EnsureParentDirectoryExists(JoinPathW(baseDir, L".dirprobe"));
    }

    HANDLE hFile = CreateFileW(
        outputManifestPath.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        errorOut = "failed to open manifest for writing";
        Logf("SaveTextureTemplatePack: CreateFileW failed for manifest '%S'", outputManifestPath.c_str());
        LogLastError("CreateFileW(manifest)");
        return false;
    }

    FILE* fp = _fdopen(_open_osfhandle((intptr_t)hFile, _O_WRONLY | _O_BINARY), "wb");
    if (!fp) {
        errorOut = "failed to open manifest for writing";
        LogLastError("_fdopen(manifest)");
        CloseHandle(hFile);
        return false;
    }

    fwprintf(fp, L"TEMPLATEPACK v1\n");
    fwprintf(fp, L"atlas_w=%d\n", atlas.width);
    fwprintf(fp, L"atlas_h=%d\n", atlas.height);
    fwprintf(fp, L"page_size=%d\n", pageSize);
    fwprintf(fp, L"cols=%d\n", pageCols);
    fwprintf(fp, L"rows=%d\n", pageRows);

    for (int py = 0; py < pageRows; ++py) {
        for (int px = 0; px < pageCols; ++px) {
            int pageIndex = py * pageCols + px;
            TextureImage tile;
            tile.width = pageSize;
            tile.height = pageSize;
            tile.rgba.resize((size_t)pageSize * (size_t)pageSize * 4);
            FillImage(tile, 0, 0, 0, 255);

            for (int y = 0; y < pageSize; ++y) {
                int srcY = py * pageSize + y;
                if (srcY >= atlas.height) break;
                for (int x = 0; x < pageSize; ++x) {
                    int srcX = px * pageSize + x;
                    if (srcX >= atlas.width) break;
                    size_t si = ((size_t)srcY * (size_t)atlas.width + (size_t)srcX) * 4;
                    size_t di = ((size_t)y * (size_t)pageSize + (size_t)x) * 4;
                    tile.rgba[di + 0] = atlas.rgba[si + 0];
                    tile.rgba[di + 1] = atlas.rgba[si + 1];
                    tile.rgba[di + 2] = atlas.rgba[si + 2];
                    tile.rgba[di + 3] = atlas.rgba[si + 3];
                }
            }

            std::wstring pageFile = baseName + L"_p" + Pad2W(pageIndex) + L".bmp";
            std::wstring pagePath = JoinPathW(baseDir, pageFile);
            std::string saveErr;
            if (!SaveBMPImageToFile(pagePath, tile, saveErr)) {
                errorOut = saveErr;
                fclose(fp);
                return false;
            }
            fwprintf(fp, L"page_%d=%s\n", pageIndex, pageFile.c_str());
            Logf("Texture template page saved: '%S'", pagePath.c_str());
        }
    }

    fclose(fp);
    Logf("Texture template manifest saved: '%S'", outputManifestPath.c_str());
    return true;
}



static void OpenCreateTextureDialogAndGenerateTemplate()
{
    SceneObject* obj = SelectedObject();
    LogSelectedObjectBinding("OpenCreateTextureDialogAndGenerateTemplate: before dialog");
    if (!obj) {
        MessageBoxW(g_hWnd, L"Select a model first.", L"Create texture", MB_OK | MB_ICONINFORMATION);
        Logf("Create texture: no selected model");
        return;
    }

    wchar_t fileName[MAX_PATH] = { 0 };
    std::wstring defaultDir = GetDirectoryFromPath(obj->sourcePathW);
    std::wstring defaultFile = SanitizeFileNamePart(MakeTemplateBaseName(obj->sourcePathW)) + L"_manifest.txt";
    wcsncpy_s(fileName, MAX_PATH, defaultFile.c_str(), _TRUNCATE);

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Texture pack manifest (*.txt)\0*.txt\0All files (*.*)\0*.*\0\0";
    ofn.lpstrInitialDir = defaultDir.empty() ? nullptr : defaultDir.c_str();
    ofn.lpstrTitle = L"Save generated texture template pack";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_EXPLORER | OFN_NOCHANGEDIR;

    Logf("Create texture: showing save dialog");
    if (!GetSaveFileNameW(&ofn)) {
        DWORD err = CommDlgExtendedError();
        if (err != 0) Logf("Create texture dialog error=0x%08lX", (unsigned long)err);
        else Logf("Create texture: canceled");
        return;
    }

    TextureImage atlas;
    int pageSize = 0, pageCols = 0, pageRows = 0;
    std::string genErr;
    if (!GenerateTextureTemplateFromMesh(obj->mesh, atlas, pageSize, pageCols, pageRows, genErr)) {
        MessageBoxA(g_hWnd, genErr.c_str(), "Texture template generation failed", MB_OK | MB_ICONERROR);
        Logf("Create texture failed: %s", genErr.c_str());
        return;
    }

    std::string saveErr;
    if (!SaveTextureTemplatePack(fileName, atlas, pageSize, pageCols, pageRows, saveErr)) {
        MessageBoxA(g_hWnd, saveErr.c_str(), "Texture template save failed", MB_OK | MB_ICONERROR);
        Logf("Create texture save failed: %s", saveErr.c_str());
        return;
    }

    std::wstring msg = L"Texture template pack saved successfully.\n\n";
    msg += fileName;
    MessageBoxW(g_hWnd, msg.c_str(), L"Create texture", MB_OK | MB_ICONINFORMATION);
    Logf("Create texture complete for '%S'", fileName);
}


static void OpenPaintedTextureDialogAndApplyToSelected()
{
    SceneObject* obj = SelectedObject();
    LogSelectedObjectBinding("OpenPaintedTextureDialogAndApplyToSelected: before dialog");
    if (!obj) {
        MessageBoxW(g_hWnd, L"Select a model first.", L"Load painted texture", MB_OK | MB_ICONINFORMATION);
        Logf("Load painted texture: no selected model");
        return;
    }

    wchar_t fileName[MAX_PATH] = { 0 };
    if (!obj->textureSourceW.empty()) {
        std::wstring seeded = obj->textureSourceW;
        if (GetExtensionLowerW(seeded) == L".txt") {
            seeded = JoinPathW(GetDirectoryFromPath(seeded), SanitizeFileNamePart(GetFileNameNoExt(seeded)) + L".bmp");
        }
        wcsncpy_s(fileName, MAX_PATH, seeded.c_str(), _TRUNCATE);
    }
    else if (!obj->sourcePathW.empty()) {
        std::wstring def = JoinPathW(GetDirectoryFromPath(obj->sourcePathW), SanitizeFileNamePart(MakeTemplateBaseName(obj->sourcePathW)) + L"_manifest.txt");
        wcsncpy_s(fileName, MAX_PATH, def.c_str(), _TRUNCATE);
    }

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Texture pack manifest (*.txt)\0*.txt\0BMP image (*.bmp)\0*.bmp\0All files (*.*)\0*.*\0\0";
    ofn.lpstrTitle = L"Load painted texture pack or BMP";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;

    Logf("Load painted texture: showing open dialog");
    if (!GetOpenFileNameW(&ofn)) {
        DWORD err = CommDlgExtendedError();
        if (err != 0) Logf("Load painted texture dialog error=0x%08lX", (unsigned long)err);
        else Logf("Load painted texture: canceled");
        return;
    }

    std::wstring selected = fileName;
    std::wstring ext = GetExtensionLowerW(selected);
    TextureImage img;
    std::string err;
    bool ok = false;

    if (ext == L".txt") {
        ok = ComposeTextureAtlasFromManifest(selected, img, err);
    }
    else {
        ok = LoadBMPImageFromFile(selected, img, err);
    }

    if (!ok) {
        MessageBoxA(g_hWnd, err.c_str(), "Load painted texture failed", MB_OK | MB_ICONERROR);
        Logf("Load painted texture failed: %s", err.c_str());
        return;
    }

    GLuint tex = CreateGLTextureFromImage(img);
    if (!tex) {
        MessageBoxW(g_hWnd, L"Failed to create the OpenGL texture from the painted file(s).", L"Load painted texture", MB_OK | MB_ICONERROR);
        Logf("Load painted texture: gl texture creation failed");
        return;
    }

    ReleaseSceneTexture(*obj);
    obj->textureId = tex;
    obj->hasTexture = true;
    obj->textureSourceW = selected;
    Logf("Load painted texture applied to '%S' from '%S' texture=%u", obj->displayNameW.c_str(), selected.c_str(), (unsigned)tex);
    LogSelectedObjectBinding("Load painted texture applied");
    UpdateWindowTitle();
    InvalidateRect(g_hWnd, NULL, FALSE);
}

// ------------------------------------------------------------
// Camera
// ------------------------------------------------------------
struct Camera {
    Vec3 position;
    float yaw;
    float pitch;

    Camera()
        : position(0.0f, 1.5f, 8.0f), yaw(0.0f), pitch(0.0f) {}

    Vec3 Forward() const {
        float cp = std::cos(pitch);
        float sp = std::sin(pitch);
        float cy = std::cos(yaw);
        float sy = std::sin(yaw);
        return Normalize(Vec3(sy * cp, sp, -cy * cp));
    }

    Vec3 Right() const {
        return Normalize(Cross(Forward(), Vec3(0, 1, 0)));
    }

    void Update(float dt, const bool* keys) {
        const float moveSpeed = 4.0f;
        const float fastSpeed = 10.0f;
        float speed = (keys[VK_SHIFT] ? fastSpeed : moveSpeed) * dt;
        Vec3 f = Forward();
        Vec3 r = Right();
        Vec3 u = Vec3(0, 1, 0);

        if (keys['W']) position = position + f * speed;
        if (keys['S']) position = position - f * speed;
        if (keys['D']) position = position + r * speed;
        if (keys['A']) position = position - r * speed;
        if (keys['E']) position = position + u * speed;
        if (keys['Q']) position = position - u * speed;
    }

    void Zoom(float amount)
    {
        Vec3 f = Forward();
        position = position + f * amount;
    }
};



static std::wstring GetBaseName(const std::wstring& path)
{
    size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return path;
    return path.substr(slash + 1);
}

static std::string WStringToUtf8(const std::wstring& ws)
{
    if (ws.empty()) return std::string();
    int needed = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, NULL, 0, NULL, NULL);
    if (needed <= 0) return std::string();
    std::string out;
    out.resize((size_t)needed - 1);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &out[0], needed, NULL, NULL);
    return out;
}

static void ReleaseSceneTexture(SceneObject& obj)
{
    if (obj.textureId != 0) {
        glDeleteTextures(1, &obj.textureId);
        Logf("ReleaseSceneTexture: deleted texture %u", (unsigned)obj.textureId);
        obj.textureId = 0;
    }
    obj.hasTexture = false;
    obj.textureSourceW.clear();
}

static bool ApplyTextureToSceneObjectFromImagePath(SceneObject& obj, const std::wstring& imagePath, const char* sourceTag)
{
    if (imagePath.empty()) return false;
    std::wstring ext = GetExtensionLowerW(imagePath);
    if (ext != L".bmp") {
        Logf("%s texture loader currently supports BMP only: '%S'", sourceTag ? sourceTag : "Texture", imagePath.c_str());
        return false;
    }

    TextureImage img;
    std::string err;
    if (!LoadBMPImageFromFile(imagePath, img, err)) {
        Logf("%s texture load failed for '%S': %s", sourceTag ? sourceTag : "Texture", imagePath.c_str(), err.c_str());
        return false;
    }

    if (obj.textureId != 0) {
        ReleaseSceneTexture(obj);
    }

    GLuint tex = CreateGLTextureFromImage(img);
    if (!tex) {
        Logf("%s texture creation failed for '%S'", sourceTag ? sourceTag : "Texture", imagePath.c_str());
        return false;
    }

    obj.textureId = tex;
    obj.hasTexture = true;
    obj.textureSourceW = imagePath;
    Logf("%s texture applied: '%S' -> texture %u (%dx%d)", sourceTag ? sourceTag : "Texture", imagePath.c_str(), (unsigned)tex, img.width, img.height);
    return true;
}

// ------------------------------------------------------------
// Global app state
// ------------------------------------------------------------
// static HWND   g_hWnd = NULL; // moved earlier for forward visibility
static HDC    g_hDC = NULL;
static HGLRC  g_hRC = NULL;
static int    g_width = 1280;
static int    g_height = 720;
static int    g_panelWidth = 340;
static bool   g_keys[256] = { false };
static bool   g_mouseDown = false;
static POINT  g_lastMouse = { 0, 0 };
static Camera g_camera;
static GLuint g_fontBase = 0;
static double g_lastTime = 0.0;
static double g_fpsAccum = 0.0;
static int    g_fpsFrames = 0;
static float  g_fps = 0.0f;
static bool   g_running = true;
static HMENU  g_mainMenu = NULL;
static HMENU  g_modelMenu = NULL;
static HMENU  g_textureMenu = NULL;
static HMENU  g_createTextureMenu = NULL;
static HMENU  g_lightsMenu = NULL;

static std::string g_glVendor;
static std::string g_glRenderer;
static std::string g_glVersion;
static std::string g_glShading;
static std::string g_glExtensions;

static void OpenFileDialogAndLoad();
static void OpenTextureDialogAndApplyToSelected(bool loadFSH);
static void OpenCreateTextureDialogAndGenerateTemplate();
static void OpenPaintedTextureDialogAndApplyToSelected();
static void HandleMenuLightToggle(UINT cmd);
static void HandlePanelCommand(int cmd);
static SceneObject* SelectedObject();
static void SelectObject(int index);
static void UpdateWindowTitle();
static void LogSelectedObjectBinding(const char* tag);
static bool TryAutoLoadSiblingMTL(SceneObject& obj);
static void ReleaseSceneTexture(struct SceneObject& obj);
static bool ApplyTextureToSceneObjectFromImagePath(struct SceneObject& obj, const std::wstring& imagePath, const char* sourceTag);

// ------------------------------------------------------------
// Menu / command IDs
// ------------------------------------------------------------
enum {
    ID_FILE_OPEN = 1001,
    ID_FILE_EXIT = 1002,

    ID_SCENE_ADD = 2001,
    ID_SCENE_RELOAD_SELECTED = 2002,
    ID_SCENE_DELETE_SELECTED = 2003,
    ID_SCENE_RESET_CAMERA = 2004,
    ID_SCENE_SELECT_NEXT = 2005,
    ID_SCENE_SELECT_PREV = 2006,

    ID_FSH_LOAD = 3001,
    ID_MTL_LOAD = 3002,

    ID_TEXTURE_CREATE_TEMPLATE = 3501,
    ID_TEXTURE_LOAD_PAINTER = 3502,

    ID_LIGHT_AMBIENT_TOGGLE = 4001,
    ID_LIGHT_DIRECTIONAL_TOGGLE = 4002,
};

enum TransformAction {
    TA_MoveXNeg,
    TA_MoveXPos,
    TA_MoveYNeg,
    TA_MoveYPos,
    TA_MoveZNeg,
    TA_MoveZPos,
    TA_RotXNeg,
    TA_RotXPos,
    TA_RotYNeg,
    TA_RotYPos,
    TA_RotZNeg,
    TA_RotZPos,
    TA_ScaleDown,
    TA_ScaleUp,
};

enum PanelCommand {
    PC_None = 0,
    PC_AmbientXNeg = 5001,
    PC_AmbientXPos,
    PC_AmbientYNeg,
    PC_AmbientYPos,
    PC_AmbientZNeg,
    PC_AmbientZPos,
    PC_DirectionalXNeg = 5011,
    PC_DirectionalXPos,
    PC_DirectionalYNeg,
    PC_DirectionalYPos,
    PC_DirectionalZNeg,
    PC_DirectionalZPos,
};

struct Box2i {
    int x0, y0, x1, y1;
};

struct UIHit {
    enum Kind { ObjectRow, CommandButton } kind;
    Box2i box;
    int index;
    int command;
};

static std::vector<UIHit> g_uiHits;

static int SceneViewportWidth()
{
    int w = g_width - g_panelWidth;
    if (w < 1) w = 1;
    return w;
}

static bool PointInBox(int x, int y, const Box2i& b)
{
    return x >= b.x0 && x <= b.x1 && y >= b.y0 && y <= b.y1;
}

static void UpdateFPS(float dt)
{
    g_fpsAccum += dt;
    ++g_fpsFrames;
    if (g_fpsAccum >= 0.5) {
        g_fps = (float)(g_fpsFrames / g_fpsAccum);
        g_fpsAccum = 0.0;
        g_fpsFrames = 0;
    }
}

static void InitFont(HDC hDC)
{
    Logf("InitFont: start");
    HFONT hFont = CreateFontA(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        FF_DONTCARE, "Consolas");

    if (!hFont) {
        LogLastError("CreateFontA");
        return;
    }

    SelectObject(hDC, hFont);
    g_fontBase = glGenLists(96);
    if (g_fontBase != 0) {
        if (!wglUseFontBitmapsA(hDC, 32, 96, g_fontBase)) {
            LogLastError("wglUseFontBitmapsA");
            glDeleteLists(g_fontBase, 96);
            g_fontBase = 0;
        }
        else {
            Logf("InitFont: created font base %u", (unsigned)g_fontBase);
        }
    }
    else {
        Logf("InitFont: glGenLists returned 0");
    }

    DeleteObject(hFont);
}

static void Set2DProjection(int width, int height)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, 0, height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void DrawText2D(int x, int y, const std::string& text)
{
    if (g_fontBase == 0) return;
    glRasterPos2i(x, y);
    glListBase(g_fontBase - 32);
    glCallLists((GLsizei)text.size(), GL_UNSIGNED_BYTE, text.c_str());
}

static void DrawFilledRect(int x0, int y0, int x1, int y1, float r, float g, float b)
{
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2i(x0, y0);
    glVertex2i(x1, y0);
    glVertex2i(x1, y1);
    glVertex2i(x0, y1);
    glEnd();
}

static void DrawRectOutline(int x0, int y0, int x1, int y1, float r, float g, float b)
{
    glColor3f(r, g, b);
    glBegin(GL_LINE_LOOP);
    glVertex2i(x0, y0);
    glVertex2i(x1, y0);
    glVertex2i(x1, y1);
    glVertex2i(x0, y1);
    glEnd();
}

static void DrawGrid(int halfSize, float step)
{
    glDisable(GL_LIGHTING);
    glColor3f(0.25f, 0.25f, 0.25f);

    glBegin(GL_LINES);
    for (int i = -halfSize; i <= halfSize; ++i) {
        float p = i * step;
        glVertex3f(p, 0.0f, -halfSize * step);
        glVertex3f(p, 0.0f, halfSize * step);

        glVertex3f(-halfSize * step, 0.0f, p);
        glVertex3f(halfSize * step, 0.0f, p);
    }
    glEnd();
}

static void DrawModel(const SceneObject& obj, bool selected)
{
    if (obj.mesh.vertices.empty()) return;

    glEnable(GL_LIGHTING);

    if (obj.hasTexture && obj.textureId != 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, obj.textureId);
        glColor3f(1.0f, 1.0f, 1.0f);
    }
    else {
        glDisable(GL_TEXTURE_2D);
        if (selected) glColor3f(1.0f, 0.92f, 0.62f);
        else glColor3f(0.82f, 0.82f, 0.88f);
    }

    glPushMatrix();
    glTranslatef(obj.position.x, obj.position.y, obj.position.z);
    glRotatef(obj.rotation.z, 0.0f, 0.0f, 1.0f);
    glRotatef(obj.rotation.y, 0.0f, 1.0f, 0.0f);
    glRotatef(obj.rotation.x, 1.0f, 0.0f, 0.0f);
    glScalef(obj.meshScale * obj.scale.x, obj.meshScale * obj.scale.y, obj.meshScale * obj.scale.z);
    glTranslatef(-obj.meshCenter.x, -obj.meshCenter.y, -obj.meshCenter.z);

    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < obj.mesh.vertices.size(); ++i) {
        const Vertex& v = obj.mesh.vertices[i];
        if (obj.hasTexture && obj.textureId != 0) {
            glTexCoord2f(v.uv.x, v.uv.y);
        }
        glNormal3f(v.normal.x, v.normal.y, v.normal.z);
        glVertex3f(v.pos.x, v.pos.y, v.pos.z);
    }
    glEnd();

    glPopMatrix();

    if (obj.hasTexture && obj.textureId != 0) {
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }
}

static void SelectObject(int index)
{
    if (index < 0 || index >= (int)g_sceneObjects.size()) {
        g_selectedObject = -1;
        Logf("SelectObject: cleared selection");
        return;
    }
    g_selectedObject = index;
    Logf("SelectObject: selected %d '%S' source='%S'", index, g_sceneObjects[index].displayNameW.c_str(), g_sceneObjects[index].sourcePathW.c_str());
    LogSelectedObjectBinding("SelectObject");
}

static SceneObject* SelectedObject()
{
    if (g_selectedObject < 0 || g_selectedObject >= (int)g_sceneObjects.size()) return NULL;
    return &g_sceneObjects[g_selectedObject];
}

static void LogSelectedObjectBinding(const char* tag)
{
    SceneObject* obj = SelectedObject();
    if (!obj) {
        Logf("%s: no selected model", tag ? tag : "Selection");
        return;
    }

    Logf("%s: selected=%d name='%S' source='%S' texture='%S' hasTexture=%s texId=%u",
        tag ? tag : "Selection",
        g_selectedObject,
        obj->displayNameW.c_str(),
        obj->sourcePathW.c_str(),
        obj->textureSourceW.c_str(),
        obj->hasTexture ? "yes" : "no",
        (unsigned)obj->textureId);
}

static bool TryAutoLoadSiblingMTL(SceneObject& obj)
{
    if (obj.sourcePathW.empty()) {
        Logf("TryAutoLoadSiblingMTL: object has no source path");
        return false;
    }

    std::wstring mtlPath = ChangeExtensionW(obj.sourcePathW, L".mtl");
    Logf("TryAutoLoadSiblingMTL: probing '%S'", mtlPath.c_str());

    if (!FileExistsW(mtlPath)) {
        Logf("TryAutoLoadSiblingMTL: not found");
        return false;
    }

    std::wstring texturePath;
    std::string err;
    if (!ParseMTLTexturePath(mtlPath, texturePath, err)) {
        Logf("TryAutoLoadSiblingMTL: parse failed (%s)", err.c_str());
        return false;
    }

    if (!ApplyTextureToSceneObjectFromImagePath(obj, texturePath, "AutoMTL")) {
        Logf("TryAutoLoadSiblingMTL: texture apply failed");
        return false;
    }

    obj.textureSourceW = mtlPath;
    Logf("TryAutoLoadSiblingMTL: success '%S' -> '%S'", mtlPath.c_str(), texturePath.c_str());
    return true;
}

static void UpdateWindowTitle()
{
    if (!g_hWnd) return;
    std::wstring title = L"OBJ scene demo";
    if (!g_sceneObjects.empty()) {
        title += L" - ";
        title += std::to_wstring((int)g_sceneObjects.size());
        title += L" models";
    }
    if (g_selectedObject >= 0 && g_selectedObject < (int)g_sceneObjects.size()) {
        title += L" | selected: ";
        title += g_sceneObjects[g_selectedObject].displayNameW;
    }
    SetWindowTextW(g_hWnd, title.c_str());
}

static void ResetCameraForScene()
{
    g_camera.position = Vec3(0.0f, 1.5f, 8.0f);
    g_camera.yaw = 0.0f;
    g_camera.pitch = 0.0f;
    Logf("Camera reset: pos=(%.3f, %.3f, %.3f)", g_camera.position.x, g_camera.position.y, g_camera.position.z);
}

static void AddLoadedMeshToScene(const Mesh& mesh, const wchar_t* path)
{
    SceneObject obj;
    obj.mesh = mesh;
    obj.sourcePathW = path ? path : L"";
    obj.displayNameW = path ? GetBaseName(path) : L"model";
    obj.meshCenter = MeshCenter(mesh);
    obj.meshScale = MeshScaleToUnit(mesh);
    obj.rotation = Vec3(0, 0, 0);
    obj.scale = Vec3(1, 1, 1);
    obj.position = Vec3((float)g_sceneObjects.size() * 3.0f, 0.0f, 0.0f);

    g_sceneObjects.push_back(obj);
    g_selectedObject = (int)g_sceneObjects.size() - 1;
    Logf("Scene add: '%S' now count=%u selected=%d", path ? path : L"(null)", (unsigned)g_sceneObjects.size(), g_selectedObject);
    UpdateWindowTitle();

    if (g_sceneObjects.size() == 1) {
        ResetCameraForScene();
    }
}

static void LoadModelAndReport(const wchar_t* path)
{
    if (!path || !path[0]) {
        Logf("LoadModelAndReport: empty path");
        return;
    }

    Logf("LoadModelAndReport: requested model '%S'", path);
    Mesh mesh;
    std::string err;
    Logf("LoadModelAndReport: begin");
    bool ok = LoadOBJFromFile(path, mesh, err);
    if (!ok) {
        char msg[1024];
#if defined(_MSC_VER)
        sprintf_s(msg, "Failed to load OBJ.\n\nPath: %S\nError: %s", path, err.c_str());
#else
        snprintf(msg, sizeof(msg), "Failed to load OBJ.\n\nPath: %S\nError: %s", path, err.c_str());
#endif
        Logf("LoadModelAndReport: failure for '%S' (%s)", path, err.c_str());
        MessageBoxA(g_hWnd, msg, "OBJ load error", MB_OK | MB_ICONERROR);
        return;
    }

    AddLoadedMeshToScene(mesh, path);
    g_currentObjPath = path;

    SceneObject* obj = SelectedObject();
    if (obj) {
        TryAutoLoadSiblingMTL(*obj);
    }

    Logf("LoadModelAndReport: success '%S'", path);
    LogSelectedObjectBinding("LoadModelAndReport");
}

static void ReloadSelectedModel()
{
    SceneObject* obj = SelectedObject();
    if (!obj) {
        Logf("ReloadSelectedModel: no selection");
        return;
    }
    if (obj->sourcePathW.empty()) {
        Logf("ReloadSelectedModel: selected object has no source path");
        return;
    }

    Mesh mesh;
    std::string err;
    Logf("ReloadSelectedModel: '%S'", obj->sourcePathW.c_str());
    if (!LoadOBJFromFile(obj->sourcePathW.c_str(), mesh, err)) {
        Logf("ReloadSelectedModel failed: %s", err.c_str());
        return;
    }
    obj->mesh = mesh;
    obj->meshCenter = MeshCenter(mesh);
    obj->meshScale = MeshScaleToUnit(mesh);
    Logf("ReloadSelectedModel: done");
}

static void DeleteSelectedModel()
{
    if (g_selectedObject < 0 || g_selectedObject >= (int)g_sceneObjects.size()) {
        Logf("DeleteSelectedModel: nothing selected");
        return;
    }
    Logf("DeleteSelectedModel: removing %d '%S'", g_selectedObject, g_sceneObjects[g_selectedObject].displayNameW.c_str());
    ReleaseSceneTexture(g_sceneObjects[g_selectedObject]);
    g_sceneObjects.erase(g_sceneObjects.begin() + g_selectedObject);
    if (g_sceneObjects.empty()) g_selectedObject = -1;
    else if (g_selectedObject >= (int)g_sceneObjects.size()) g_selectedObject = (int)g_sceneObjects.size() - 1;
    UpdateWindowTitle();
}

static void ResetSelectedModelTransform()
{
    SceneObject* obj = SelectedObject();
    if (!obj) return;
    obj->position = Vec3(0, 0, 0);
    obj->rotation = Vec3(0, 0, 0);
    obj->scale = Vec3(1, 1, 1);
    Logf("ResetSelectedModelTransform: selected=%d", g_selectedObject);
}

static void ApplyTransformToSelected(TransformAction a)
{
    SceneObject* obj = SelectedObject();
    if (!obj) {
        Logf("ApplyTransformToSelected: no selection");
        return;
    }

    const float move = 0.25f;
    const float rot = 10.0f;
    const float scaleStep = 0.10f;

    switch (a) {
    case TA_MoveXNeg: obj->position.x -= move; break;
    case TA_MoveXPos: obj->position.x += move; break;
    case TA_MoveYNeg: obj->position.y -= move; break;
    case TA_MoveYPos: obj->position.y += move; break;
    case TA_MoveZNeg: obj->position.z -= move; break;
    case TA_MoveZPos: obj->position.z += move; break;
    case TA_RotXNeg:  obj->rotation.x -= rot;  break;
    case TA_RotXPos:  obj->rotation.x += rot;  break;
    case TA_RotYNeg:  obj->rotation.y -= rot;  break;
    case TA_RotYPos:  obj->rotation.y += rot;  break;
    case TA_RotZNeg:  obj->rotation.z -= rot;  break;
    case TA_RotZPos:  obj->rotation.z += rot;  break;
    case TA_ScaleDown:
        obj->scale.x = std::max(0.1f, obj->scale.x - scaleStep);
        obj->scale.y = std::max(0.1f, obj->scale.y - scaleStep);
        obj->scale.z = std::max(0.1f, obj->scale.z - scaleStep);
        break;
    case TA_ScaleUp:
        obj->scale.x += scaleStep;
        obj->scale.y += scaleStep;
        obj->scale.z += scaleStep;
        break;
    }

    Logf("Transform selected %d => pos(%.2f,%.2f,%.2f) rot(%.1f,%.1f,%.1f) scale(%.2f,%.2f,%.2f)",
        g_selectedObject,
        obj->position.x, obj->position.y, obj->position.z,
        obj->rotation.x, obj->rotation.y, obj->rotation.z,
        obj->scale.x, obj->scale.y, obj->scale.z);
}

static void HandleSceneCommand(UINT cmd)
{
    switch (cmd) {
    case ID_SCENE_ADD:
        Logf("Scene command: Add model");
        break;
    case ID_SCENE_RELOAD_SELECTED:
        Logf("Scene command: Reload selected");
        ReloadSelectedModel();
        break;
    case ID_SCENE_DELETE_SELECTED:
        Logf("Scene command: Delete selected");
        DeleteSelectedModel();
        break;
    case ID_SCENE_RESET_CAMERA:
        Logf("Scene command: Reset camera");
        ResetCameraForScene();
        break;
    case ID_SCENE_SELECT_NEXT:
        if (!g_sceneObjects.empty()) SelectObject((g_selectedObject + 1) % (int)g_sceneObjects.size());
        break;
    case ID_SCENE_SELECT_PREV:
        if (!g_sceneObjects.empty()) SelectObject((g_selectedObject - 1 + (int)g_sceneObjects.size()) % (int)g_sceneObjects.size());
        break;
    }
}

// ------------------------------------------------------------
// UI drawing / interaction
// ------------------------------------------------------------
static void AddHit(const UIHit& h)
{
    g_uiHits.push_back(h);
}

static bool HandlePanelClick(int x, int y)
{
    for (size_t i = 0; i < g_uiHits.size(); ++i) {
        if (PointInBox(x, y, g_uiHits[i].box)) {
            const UIHit& h = g_uiHits[i];
            if (h.kind == UIHit::ObjectRow) {
                SelectObject(h.index);
            }
            else if (h.kind == UIHit::CommandButton) {
                Logf("Panel button command=%d", h.command);
                HandlePanelCommand(h.command);
            }
            return true;
        }
    }
    return true;
}

static void AdjustLightAxis(LightState& light, int axis, float delta)
{
    if (axis == 0) light.position.x += delta;
    else if (axis == 1) light.position.y += delta;
    else light.position.z += delta;
}

static void HandleMenuLightToggle(UINT cmd)
{
    if (cmd == ID_LIGHT_AMBIENT_TOGGLE) {
        g_ambientLight.enabled = !g_ambientLight.enabled;
        Logf("Light toggle: ambient => %s", g_ambientLight.enabled ? "on" : "off");
    }
    else if (cmd == ID_LIGHT_DIRECTIONAL_TOGGLE) {
        g_directionalLight.enabled = !g_directionalLight.enabled;
        Logf("Light toggle: directional => %s", g_directionalLight.enabled ? "on" : "off");
    }

    if (g_lightsMenu) {
        CheckMenuItem(g_lightsMenu, ID_LIGHT_AMBIENT_TOGGLE, MF_BYCOMMAND | (g_ambientLight.enabled ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(g_lightsMenu, ID_LIGHT_DIRECTIONAL_TOGGLE, MF_BYCOMMAND | (g_directionalLight.enabled ? MF_CHECKED : MF_UNCHECKED));
    }
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static void HandlePanelCommand(int cmd)
{
    switch (cmd) {
    case TA_MoveXNeg: case TA_MoveXPos:
    case TA_MoveYNeg: case TA_MoveYPos:
    case TA_MoveZNeg: case TA_MoveZPos:
    case TA_RotXNeg: case TA_RotXPos:
    case TA_RotYNeg: case TA_RotYPos:
    case TA_RotZNeg: case TA_RotZPos:
    case TA_ScaleDown: case TA_ScaleUp:
        ApplyTransformToSelected((TransformAction)cmd);
        break;

    case PC_AmbientXNeg: AdjustLightAxis(g_ambientLight, 0, -0.25f); Logf("Ambient light position => (%.2f, %.2f, %.2f)", g_ambientLight.position.x, g_ambientLight.position.y, g_ambientLight.position.z); break;
    case PC_AmbientXPos: AdjustLightAxis(g_ambientLight, 0, 0.25f); Logf("Ambient light position => (%.2f, %.2f, %.2f)", g_ambientLight.position.x, g_ambientLight.position.y, g_ambientLight.position.z); break;
    case PC_AmbientYNeg: AdjustLightAxis(g_ambientLight, 1, -0.25f); Logf("Ambient light position => (%.2f, %.2f, %.2f)", g_ambientLight.position.x, g_ambientLight.position.y, g_ambientLight.position.z); break;
    case PC_AmbientYPos: AdjustLightAxis(g_ambientLight, 1, 0.25f); Logf("Ambient light position => (%.2f, %.2f, %.2f)", g_ambientLight.position.x, g_ambientLight.position.y, g_ambientLight.position.z); break;
    case PC_AmbientZNeg: AdjustLightAxis(g_ambientLight, 2, -0.25f); Logf("Ambient light position => (%.2f, %.2f, %.2f)", g_ambientLight.position.x, g_ambientLight.position.y, g_ambientLight.position.z); break;
    case PC_AmbientZPos: AdjustLightAxis(g_ambientLight, 2, 0.25f); Logf("Ambient light position => (%.2f, %.2f, %.2f)", g_ambientLight.position.x, g_ambientLight.position.y, g_ambientLight.position.z); break;

    case PC_DirectionalXNeg: AdjustLightAxis(g_directionalLight, 0, -0.25f); Logf("Directional light position => (%.2f, %.2f, %.2f)", g_directionalLight.position.x, g_directionalLight.position.y, g_directionalLight.position.z); break;
    case PC_DirectionalXPos: AdjustLightAxis(g_directionalLight, 0, 0.25f); Logf("Directional light position => (%.2f, %.2f, %.2f)", g_directionalLight.position.x, g_directionalLight.position.y, g_directionalLight.position.z); break;
    case PC_DirectionalYNeg: AdjustLightAxis(g_directionalLight, 1, -0.25f); Logf("Directional light position => (%.2f, %.2f, %.2f)", g_directionalLight.position.x, g_directionalLight.position.y, g_directionalLight.position.z); break;
    case PC_DirectionalYPos: AdjustLightAxis(g_directionalLight, 1, 0.25f); Logf("Directional light position => (%.2f, %.2f, %.2f)", g_directionalLight.position.x, g_directionalLight.position.y, g_directionalLight.position.z); break;
    case PC_DirectionalZNeg: AdjustLightAxis(g_directionalLight, 2, -0.25f); Logf("Directional light position => (%.2f, %.2f, %.2f)", g_directionalLight.position.x, g_directionalLight.position.y, g_directionalLight.position.z); break;
    case PC_DirectionalZPos: AdjustLightAxis(g_directionalLight, 2, 0.25f); Logf("Directional light position => (%.2f, %.2f, %.2f)", g_directionalLight.position.x, g_directionalLight.position.y, g_directionalLight.position.z); break;
    default:
        break;
    }
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static void DrawPanelButton(int x0, int y0, int w, int h, const std::string& label, int command, bool selected = false)
{
    DrawFilledRect(x0, y0, x0 + w, y0 + h, selected ? 0.28f : 0.18f, selected ? 0.28f : 0.18f, selected ? 0.32f : 0.22f);
    DrawRectOutline(x0, y0, x0 + w, y0 + h, 0.45f, 0.45f, 0.50f);
    DrawText2D(x0 + 6, y0 + 4, label);

    UIHit hit;
    hit.kind = UIHit::CommandButton;
    hit.box = { x0, y0, x0 + w, y0 + h };
    hit.index = -1;
    hit.command = command;
    AddHit(hit);
}

static void DrawObjectRow(int x0, int y0, int w, int h, int index, const std::wstring& name, bool selected)
{
    DrawFilledRect(x0, y0, x0 + w, y0 + h, selected ? 0.22f : 0.12f, selected ? 0.30f : 0.14f, selected ? 0.36f : 0.16f);
    DrawRectOutline(x0, y0, x0 + w, y0 + h, selected ? 0.75f : 0.35f, selected ? 0.75f : 0.35f, selected ? 0.85f : 0.40f);
    DrawText2D(x0 + 6, y0 + 4, WStringToUtf8(name));

    UIHit hit;
    hit.kind = UIHit::ObjectRow;
    hit.box = { x0, y0, x0 + w, y0 + h };
    hit.index = index;
    hit.command = 0;
    AddHit(hit);
}

static void DrawPanel()
{
    int sceneW = SceneViewportWidth();
    int panelW = g_width - sceneW;
    if (panelW <= 0) return;

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glViewport(sceneW, 0, panelW, g_height);
    Set2DProjection(panelW, g_height);
    g_uiHits.clear();

    DrawFilledRect(0, 0, panelW, g_height, 0.11f, 0.12f, 0.14f);
    DrawRectOutline(0, 0, panelW - 1, g_height - 1, 0.30f, 0.30f, 0.34f);

    int x = 12;
    int y = g_height - 24;

    DrawText2D(x, y, "Scene");
    y -= 20;

    char info[256];
    sprintf(info, "Lights: ambient %s | directional %s",
        g_ambientLight.enabled ? "on" : "off",
        g_directionalLight.enabled ? "on" : "off");
    DrawText2D(x, y, info);
    y -= 18;

    sprintf(info, "Models: %u", (unsigned)g_sceneObjects.size());
    DrawText2D(x, y, info);
    y -= 18;

    if (g_selectedObject >= 0 && g_selectedObject < (int)g_sceneObjects.size()) {
        DrawText2D(x, y, std::string("Selected: ") + WStringToUtf8(g_sceneObjects[g_selectedObject].displayNameW));
        y -= 18;
        if (g_sceneObjects[g_selectedObject].hasTexture) {
            DrawText2D(x, y, std::string("Texture: ") + WStringToUtf8(g_sceneObjects[g_selectedObject].textureSourceW));
        }
        else {
            DrawText2D(x, y, "Texture: none");
        }
    }
    else {
        DrawText2D(x, y, "Selected: none");
        y -= 18;
        DrawText2D(x, y, "Texture: n/a");
    }
    y -= 26;

    DrawText2D(x, y, "Objects");
    y -= 18;

    int listH = 22;
    int listW = std::max(40, panelW - 24);
    int visible = std::min((int)g_sceneObjects.size(), 8);
    for (int i = 0; i < visible; ++i) {
        int rowY = y - i * (listH + 4) - listH;
        DrawObjectRow(x, rowY, listW, listH, i, g_sceneObjects[i].displayNameW, i == g_selectedObject);
    }
    y -= visible * (listH + 4);
    y -= 8;

    DrawText2D(x, y, "Selected tools");
    y -= 18;

    int bw = std::max(40, (panelW - 36) / 3);
    int bh = 22;
    int gap = 6;
    int bx = x;
    int by = y - bh;

    DrawPanelButton(bx, by, bw, bh, "X-", TA_MoveXNeg);
    DrawPanelButton(bx + bw + gap, by, bw, bh, "X+", TA_MoveXPos);
    DrawPanelButton(bx + (bw + gap) * 2, by, bw, bh, "Y-", TA_MoveYNeg);
    by -= bh + gap;
    DrawPanelButton(bx, by, bw, bh, "Y+", TA_MoveYPos);
    DrawPanelButton(bx + bw + gap, by, bw, bh, "Z-", TA_MoveZNeg);
    DrawPanelButton(bx + (bw + gap) * 2, by, bw, bh, "Z+", TA_MoveZPos);
    by -= bh + gap;
    DrawPanelButton(bx, by, bw, bh, "Rx-", TA_RotXNeg);
    DrawPanelButton(bx + bw + gap, by, bw, bh, "Rx+", TA_RotXPos);
    DrawPanelButton(bx + (bw + gap) * 2, by, bw, bh, "Ry-", TA_RotYNeg);
    by -= bh + gap;
    DrawPanelButton(bx, by, bw, bh, "Ry+", TA_RotYPos);
    DrawPanelButton(bx + bw + gap, by, bw, bh, "Rz-", TA_RotZNeg);
    DrawPanelButton(bx + (bw + gap) * 2, by, bw, bh, "Rz+", TA_RotZPos);
    by -= bh + gap;
    int scaleW = std::max(40, (panelW - 36) / 2);
    DrawPanelButton(bx, by, scaleW, bh, "Scale -", TA_ScaleDown);
    DrawPanelButton(bx + scaleW + gap, by, scaleW, bh, "Scale +", TA_ScaleUp);

    y = by - 20;
    DrawText2D(x, y, "Lights");
    y -= 18;

    DrawText2D(x, y, "Menu toggles: ambient / directional");
    y -= 18;

    if (g_ambientLight.enabled) {
        sprintf(info, "Ambient pos (debug): %.2f %.2f %.2f",
            g_ambientLight.position.x, g_ambientLight.position.y, g_ambientLight.position.z);
        DrawText2D(x, y, info);
        y -= 18;
        DrawPanelButton(x, y - 22, 64, 22, "Ax-", PC_AmbientXNeg);
        DrawPanelButton(x + 70, y - 22, 64, 22, "Ax+", PC_AmbientXPos);
        DrawPanelButton(x + 140, y - 22, 64, 22, "Ay-", PC_AmbientYNeg);
        y -= 28;
        DrawPanelButton(x, y - 22, 64, 22, "Ay+", PC_AmbientYPos);
        DrawPanelButton(x + 70, y - 22, 64, 22, "Az-", PC_AmbientZNeg);
        DrawPanelButton(x + 140, y - 22, 64, 22, "Az+", PC_AmbientZPos);
        y -= 34;
    }

    if (g_directionalLight.enabled) {
        sprintf(info, "Directional pos: %.2f %.2f %.2f",
            g_directionalLight.position.x, g_directionalLight.position.y, g_directionalLight.position.z);
        DrawText2D(x, y, info);
        y -= 18;
        DrawPanelButton(x, y - 22, 64, 22, "Dx-", PC_DirectionalXNeg);
        DrawPanelButton(x + 70, y - 22, 64, 22, "Dx+", PC_DirectionalXPos);
        DrawPanelButton(x + 140, y - 22, 64, 22, "Dy-", PC_DirectionalYNeg);
        y -= 28;
        DrawPanelButton(x, y - 22, 64, 22, "Dy+", PC_DirectionalYPos);
        DrawPanelButton(x + 70, y - 22, 64, 22, "Dz-", PC_DirectionalZNeg);
        DrawPanelButton(x + 140, y - 22, 64, 22, "Dz+", PC_DirectionalZPos);
        y -= 34;
    }

    DrawText2D(x, y, "Top menu: File / Model / Textures / Create Texture / Lights");
    y -= 16;
    DrawText2D(x, y, "RMB drag: rotate camera");
    y -= 16;
    DrawText2D(x, y, "Wheel: zoom");
}


static void DrawStatsOverlay()
{
    int sceneW = SceneViewportWidth();
    glViewport(0, 0, sceneW, g_height);
    Set2DProjection(sceneW, g_height);

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glColor3f(1.0f, 1.0f, 1.0f);

    char buf[512];
    std::string line1 = std::string("GPU: ") + g_glVendor;
    std::string line2 = std::string("Renderer: ") + g_glRenderer;
    std::string line3 = std::string("OpenGL: ") + g_glVersion;
    std::string line4 = std::string("Shading: ") + g_glShading;
    sprintf(buf, "Models: %u  Triangles: %u  Verts: %u", (unsigned)g_sceneObjects.size(),
        (unsigned)0, (unsigned)0);
    std::string line5 = buf;
    sprintf(buf, "FPS: %.1f   RMB look   Wheel zoom   WASD move   Q/E up/down   Shift fast", g_fps);
    std::string line6 = buf;

    if (!g_sceneObjects.empty()) {
        unsigned triCount = 0;
        unsigned vertCount = 0;
        for (size_t i = 0; i < g_sceneObjects.size(); ++i) {
            triCount += (unsigned)(g_sceneObjects[i].mesh.vertices.size() / 3);
            vertCount += (unsigned)g_sceneObjects[i].mesh.vertices.size();
        }
        sprintf(buf, "Models: %u  Triangles: %u  Verts: %u", (unsigned)g_sceneObjects.size(), triCount, vertCount);
        line5 = buf;
    }

    DrawText2D(8, 8, line1);
    DrawText2D(8, 26, line2);
    DrawText2D(8, 44, line3);
    DrawText2D(8, 62, line4);
    DrawText2D(8, 80, line5);
    DrawText2D(8, 98, line6);
}


///
#if 1
enum {
    ID_MODEL_CONTROL_ENABLE = 4101,
    ID_MODEL_CONTROL_DISABLE = 4102,
    ID_CAMERA_FOLLOW_ENABLE = 4103,
    ID_CAMERA_FOLLOW_DISABLE = 4104,
};

static bool  g_modelControlEnabled = false;
static bool  g_cameraFollowEnabled = false;
static double g_modelControlTimeSec = 0.0;
static float g_modelControlSpeed = 0.0f;
static Vec3  g_modelControlVelocity(0.0f, 0.0f, 0.0f);
static int   g_modelControlSelectedIndex = -1;
static Vec3  g_cameraFollowOffset(0.0f, 1.8f, 6.0f);

static void SetModelControlEnabled(bool enabled)
{
    g_modelControlEnabled = enabled;
    if (!enabled) {
        g_modelControlSpeed = 0.0f;
        g_modelControlVelocity = Vec3(0.0f, 0.0f, 0.0f);
        g_modelControlTimeSec = 0.0;
    }
    Logf("Model control: %s", enabled ? "enabled" : "disabled");
    if (g_mainMenu) {
        CheckMenuItem(g_mainMenu, ID_MODEL_CONTROL_ENABLE, MF_BYCOMMAND | (g_modelControlEnabled ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(g_mainMenu, ID_MODEL_CONTROL_DISABLE, MF_BYCOMMAND | (!g_modelControlEnabled ? MF_CHECKED : MF_UNCHECKED));
    }
}

static void SetCameraFollowEnabled(bool enabled)
{
    g_cameraFollowEnabled = enabled;
    Logf("Camera follow: %s", enabled ? "enabled" : "disabled");
    if (g_mainMenu) {
        CheckMenuItem(g_mainMenu, ID_CAMERA_FOLLOW_ENABLE, MF_BYCOMMAND | (g_cameraFollowEnabled ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(g_mainMenu, ID_CAMERA_FOLLOW_DISABLE, MF_BYCOMMAND | (!g_cameraFollowEnabled ? MF_CHECKED : MF_UNCHECKED));
    }
}

static void SyncModelControlSelection()
{
    SceneObject* obj = SelectedObject();
    if (!obj) {
        g_modelControlSelectedIndex = -1;
        Logf("Model control: no selected model");
        return;
    }

    if (g_modelControlSelectedIndex != g_selectedObject) {
        g_modelControlSelectedIndex = g_selectedObject;
        LogSelectedObjectBinding("Model control target");
    }
}

static Vec3 RotateYLocalToWorld(const Vec3& local, float yawRadians)
{
    float c = std::cos(yawRadians);
    float s = std::sin(yawRadians);
    return Vec3(
        local.x * c - local.z * s,
        local.y,
        local.x * s + local.z * c
    );
}

static void StepModelControl(float dt)
{
    if (!g_modelControlEnabled) return;

    SceneObject* obj = SelectedObject();
    if (!obj) return;

    SyncModelControlSelection();

    g_modelControlTimeSec += dt;

    const float accel = 8.0f;
    const float maxSpeed = 14.0f;
    const float brakePower = 20.0f;
    const float steeringRate = 2.0f;

    float forwardInput = 0.0f;
    float turnInput = 0.0f;
    bool braking = false;

    if (g_keys[VK_UP]) forwardInput += 1.0f;
    if (g_keys[VK_DOWN]) forwardInput -= 1.0f;
    if (g_keys[VK_LEFT]) turnInput -= 1.0f;
    if (g_keys[VK_RIGHT]) turnInput += 1.0f;
    if (g_keys[VK_SPACE]) braking = true;

    obj->rotation.y += turnInput * steeringRate * 60.0f * dt;

    if (forwardInput != 0.0f) {
        g_modelControlSpeed += forwardInput * accel * dt;
    }

    if (braking) {
        if (g_modelControlSpeed > 0.0f) {
            g_modelControlSpeed -= brakePower * dt;
            if (g_modelControlSpeed < 0.0f) g_modelControlSpeed = 0.0f;
        }
        else if (g_modelControlSpeed < 0.0f) {
            g_modelControlSpeed += brakePower * dt;
            if (g_modelControlSpeed > 0.0f) g_modelControlSpeed = 0.0f;
        }
    }

    if (g_modelControlSpeed > maxSpeed) g_modelControlSpeed = maxSpeed;
    if (g_modelControlSpeed < -maxSpeed) g_modelControlSpeed = -maxSpeed;

    float yawRad = obj->rotation.y * 0.0174532925f;
    Vec3 forwardLocal(0.0f, 0.0f, -1.0f);
    Vec3 worldForward = RotateYLocalToWorld(forwardLocal, yawRad);

    obj->position = obj->position + worldForward * (g_modelControlSpeed * dt);
    g_modelControlVelocity = worldForward * g_modelControlSpeed;

    Logf("Model control step: sel=%d speed=%.3f t=%.3f pos=(%.3f, %.3f, %.3f) rotY=%.2f",
        g_selectedObject, g_modelControlSpeed, g_modelControlTimeSec,
        obj->position.x, obj->position.y, obj->position.z, obj->rotation.y);
}

static void UpdateFollowCameraFromSelected()
{
    if (!g_cameraFollowEnabled) return;

    SceneObject* obj = SelectedObject();
    if (!obj) return;

    float yawRad = obj->rotation.y * 0.0174532925f;
    Vec3 offsetWorld = RotateYLocalToWorld(g_cameraFollowOffset, yawRad);
    g_camera.position = obj->position + offsetWorld;

    Vec3 lookAt = obj->position + Vec3(0.0f, 1.0f, 0.0f);
    Vec3 delta = lookAt - g_camera.position;

    g_camera.yaw = std::atan2(delta.x, -delta.z);
    float len = std::sqrt(delta.x * delta.x + delta.z * delta.z);
    g_camera.pitch = std::atan2(delta.y, len);

    Logf("Camera follow: target sel=%d cam=(%.3f, %.3f, %.3f) look=(%.3f, %.3f, %.3f)",
        g_selectedObject,
        g_camera.position.x, g_camera.position.y, g_camera.position.z,
        lookAt.x, lookAt.y, lookAt.z);
}

static void DrawModelControlOverlay()
{
    if (!g_modelControlEnabled) return;

    int y = 122;
    DrawText2D(8, y, "MODEL CONTROL ON");
    y += 18;
    DrawText2D(8, y, "Arrows: move / steer");
    y += 18;
    DrawText2D(8, y, "SPACE: brake");
    y += 18;

    char buf[256];
    sprintf(buf, "Speed: %.2f", g_modelControlSpeed);
    DrawText2D(8, y, buf);
    y += 18;
    sprintf(buf, "Time: %.2f", g_modelControlTimeSec);
    DrawText2D(8, y, buf);
}
#endif

// ------------------------------------------------------------
// Rendering
// ------------------------------------------------------------
static void RenderFrame()
{
    static int first = 1;
    double now = (double)GetTickCount64() * 0.001;
    if (first) {
        g_lastTime = now;
        first = 0;
        Logf("RenderFrame: first frame");
    }

    float dt = (float)(now - g_lastTime);
    g_lastTime = now;
    if (dt > 0.1f) dt = 0.1f;

    g_camera.Update(dt, g_keys);
    UpdateFPS(dt);

    int sceneW = SceneViewportWidth();
    glViewport(0, 0, sceneW, g_height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (g_height > 0) ? (double)sceneW / (double)g_height : 1.0, 0.1, 500.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    //

    SyncModelControlSelection();

    if (g_modelControlEnabled) {
        StepModelControl(dt);
    }

    if (!g_cameraFollowEnabled) {
        g_camera.Update(dt, g_keys);
    }
    else {
        UpdateFollowCameraFromSelected();
    }


    //

    Vec3 forward = g_camera.Forward();
    Vec3 up(0, 1, 0);
    Vec3 target = g_camera.position + forward;
    gluLookAt(
        g_camera.position.x, g_camera.position.y, g_camera.position.z,
        target.x, target.y, target.z,
        up.x, up.y, up.z
    );

    ApplyLighting();
    glDisable(GL_TEXTURE_2D);
    DrawGrid(20, 1.0f);

    for (size_t i = 0; i < g_sceneObjects.size(); ++i) {
        DrawModel(g_sceneObjects[i], (int)i == g_selectedObject);
    }

    DrawStatsOverlay();
    DrawPanel();

    if (!SwapBuffers(g_hDC)) {
        LogLastError("SwapBuffers");
    }

    LogOpenGLState("RenderFrame");
}

// ------------------------------------------------------------
// Window procedure
// ------------------------------------------------------------
LONG WINAPI WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_SIZE:
        g_width = LOWORD(lParam);
        g_height = HIWORD(lParam);
        if (g_height <= 0) g_height = 1;
        Logf("WM_SIZE: %dx%d", g_width, g_height);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {

         // for model control
#if 1
        case ID_MODEL_CONTROL_ENABLE:
            SetModelControlEnabled(true);
            return 0;
        case ID_MODEL_CONTROL_DISABLE:
            SetModelControlEnabled(false);
            return 0;
        case ID_CAMERA_FOLLOW_ENABLE:
            SetCameraFollowEnabled(true);
            return 0;
        case ID_CAMERA_FOLLOW_DISABLE:
            SetCameraFollowEnabled(false);
            return 0;
#endif

        case ID_FILE_OPEN:
            Logf("WM_COMMAND: File > Open");
            OpenFileDialogAndLoad();
            return 0;
        case ID_FILE_EXIT:
            Logf("WM_COMMAND: File > Exit");
            PostQuitMessage(0);
            return 0;
        case ID_SCENE_ADD:
            Logf("WM_COMMAND: Scene > Add Model");
            OpenFileDialogAndLoad();
            return 0;
        case ID_SCENE_RELOAD_SELECTED:
        case ID_SCENE_DELETE_SELECTED:
        case ID_SCENE_RESET_CAMERA:
        case ID_SCENE_SELECT_NEXT:
        case ID_SCENE_SELECT_PREV:
            HandleSceneCommand(LOWORD(wParam));
            return 0;
        case ID_FSH_LOAD:
            Logf("WM_COMMAND: Textures > Load FSH Texture Set for Selected Model");
            OpenTextureDialogAndApplyToSelected(true);
            return 0;
        case ID_MTL_LOAD:
            Logf("WM_COMMAND: Model/Textures > Load MTL Texture for Selected Model");
            OpenTextureDialogAndApplyToSelected(false);
            return 0;
        case ID_TEXTURE_CREATE_TEMPLATE:
            Logf("WM_COMMAND: Create Texture > Generate Texture Template");
            OpenCreateTextureDialogAndGenerateTemplate();
            return 0;
        case ID_TEXTURE_LOAD_PAINTER:
            Logf("WM_COMMAND: Create Texture > Load Painted Texture(s)");
            OpenPaintedTextureDialogAndApplyToSelected();
            return 0;
        case ID_LIGHT_AMBIENT_TOGGLE:
        case ID_LIGHT_DIRECTIONAL_TOGGLE:
            HandleMenuLightToggle(LOWORD(wParam));
            return 0;
        }
        return 0;

    case WM_KEYDOWN:
        if (wParam < 256) g_keys[wParam] = true;
        if (wParam == VK_ESCAPE) PostQuitMessage(0);
        return 0;

    case WM_KEYUP:
        if (wParam < 256) g_keys[wParam] = false;
        return 0;

    case WM_MOUSEWHEEL:
    {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        float zoom = (delta > 0) ? -0.8f : 0.8f;
        g_camera.Zoom(zoom);
        Logf("WM_MOUSEWHEEL: delta=%d zoom=%.2f", (int)delta, zoom);
        return 0;
    }

    case WM_RBUTTONDOWN:
    {
        int mx = GET_X_LPARAM(lParam);
        int my = GET_Y_LPARAM(lParam);
        if (mx < SceneViewportWidth()) {
            g_mouseDown = true;
            SetCapture(hWnd);
            g_lastMouse.x = mx;
            g_lastMouse.y = my;
            Logf("RButton down at %d,%d", g_lastMouse.x, g_lastMouse.y);
        }
        return 0;
    }

    case WM_RBUTTONUP:
        g_mouseDown = false;
        ReleaseCapture();
        Logf("RButton up");
        return 0;

    case WM_LBUTTONDOWN:
    {
        int mx = GET_X_LPARAM(lParam);
        int my = g_height - GET_Y_LPARAM(lParam);
        int sceneW = SceneViewportWidth();
        if (mx >= sceneW) {
            int panelX = mx - sceneW;
            Logf("Panel click at %d,%d", panelX, my);
            if (HandlePanelClick(panelX, my)) {
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }
        return 0;
    }

    case WM_MOUSEMOVE:
        if (g_mouseDown) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            int dx = x - g_lastMouse.x;
            int dy = y - g_lastMouse.y;
            g_lastMouse.x = x;
            g_lastMouse.y = y;

            const float sensitivity = 0.005f;
            g_camera.yaw += dx * sensitivity;
            g_camera.pitch -= dy * sensitivity;

            const float limit = 1.55f;
            if (g_camera.pitch > limit) g_camera.pitch = limit;
            if (g_camera.pitch < -limit) g_camera.pitch = -limit;
        }
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        RenderFrame();
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_CLOSE:
        Logf("WM_CLOSE");
        PostQuitMessage(0);
        return 0;

    case WM_DESTROY:
        Logf("WM_DESTROY");
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}



// ------------------------------------------------------------
// Window / OpenGL setup
// ------------------------------------------------------------
static HMENU CreateAppMenu()
{
    HMENU hMenu = CreateMenu();
    HMENU hFile = CreatePopupMenu();
    HMENU hModel = CreatePopupMenu();
    HMENU hTextures = CreatePopupMenu();
    HMENU hCreateTexture = CreatePopupMenu();
    HMENU hLights = CreatePopupMenu();

    g_modelMenu = hModel;
    g_textureMenu = hTextures;
    g_createTextureMenu = hCreateTexture;
    g_lightsMenu = hLights;

    AppendMenuW(hFile, MF_STRING, ID_FILE_OPEN, L"&Open...\tCtrl+O");
    AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFile, MF_STRING, ID_FILE_EXIT, L"E&xit");

    AppendMenuW(hModel, MF_STRING, ID_SCENE_ADD, L"&Load OBJ...\tCtrl+Shift+O");
    AppendMenuW(hModel, MF_STRING, ID_MTL_LOAD, L"Load &MTL for Selected Model...");
    AppendMenuW(hModel, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hModel, MF_STRING, ID_SCENE_RELOAD_SELECTED, L"&Reload Selected\tF5");
    AppendMenuW(hModel, MF_STRING, ID_SCENE_SELECT_NEXT, L"Select &Next\tTab");
    AppendMenuW(hModel, MF_STRING, ID_SCENE_SELECT_PREV, L"Select &Previous\tShift+Tab");
    AppendMenuW(hModel, MF_STRING, ID_SCENE_RESET_CAMERA, L"Reset &Camera");
    AppendMenuW(hModel, MF_STRING, ID_SCENE_DELETE_SELECTED, L"&Delete Selected");

    AppendMenuW(hTextures, MF_STRING, ID_FSH_LOAD, L"Load &FSH Texture Set for Selected Model...");
    AppendMenuW(hTextures, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hTextures, MF_STRING, ID_MTL_LOAD, L"Load &MTL Texture Again...");

    AppendMenuW(hCreateTexture, MF_STRING, ID_TEXTURE_CREATE_TEMPLATE, L"Generate Texture &Template for Selected Model...");
    AppendMenuW(hCreateTexture, MF_STRING, ID_TEXTURE_LOAD_PAINTER, L"Load &Painted Texture(s)...");

    AppendMenuW(hLights, MF_STRING, ID_LIGHT_AMBIENT_TOGGLE, L"&Ambient Light");
    AppendMenuW(hLights, MF_STRING, ID_LIGHT_DIRECTIONAL_TOGGLE, L"&Directional Light");

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFile, L"&File");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hModel, L"&Model");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTextures, L"&Textures");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hCreateTexture, L"&Create Texture");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hLights, L"&Lights");


    // for control model
    static HMENU hControl = CreatePopupMenu();
    g_mainMenu = hMenu;
    
    AppendMenuW(hControl, MF_STRING, ID_MODEL_CONTROL_ENABLE, L"Enable model control");
    AppendMenuW(hControl, MF_STRING, ID_MODEL_CONTROL_DISABLE, L"Disable model control");
    AppendMenuW(hControl, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hControl, MF_STRING, ID_CAMERA_FOLLOW_ENABLE, L"Enable follow camera");
    AppendMenuW(hControl, MF_STRING, ID_CAMERA_FOLLOW_DISABLE, L"Disable follow camera");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hControl, L"&Model Control");


    Logf("CreateAppMenu: built File/Model/Textures/Lights menus");
    return hMenu;
}

static HWND CreateOpenGLWindow(LPCWSTR title, int x, int y, int width, int height,
    BYTE type, DWORD flags, HMENU hMenu)
{
    int pf;
    HDC hDC;
    HWND hWnd;
    WNDCLASSW wc;
    PIXELFORMATDESCRIPTOR pfd;
    static HINSTANCE hInstance = 0;

    Logf("CreateOpenGLWindow: begin");

    if (!hInstance) {
        hInstance = GetModuleHandleW(NULL);
        ZeroMemory(&wc, sizeof(wc));
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = (WNDPROC)WindowProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = hInstance;
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = NULL;
        wc.lpszMenuName = NULL;
        wc.lpszClassName = L"OpenGLOBJDemo";

        if (!RegisterClassW(&wc)) {
            LogLastError("RegisterClassW");
            MessageBoxW(NULL, L"RegisterClassW failed.", L"Error", MB_OK | MB_ICONERROR);
            return NULL;
        }
        Logf("RegisterClassW: ok");
    }

    hWnd = CreateWindowW(L"OpenGLOBJDemo", title,
        WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        x, y, width, height, NULL, hMenu, hInstance, NULL);

    if (hWnd == NULL) {
        LogLastError("CreateWindowW");
        MessageBoxW(NULL, L"CreateWindowW failed.", L"Error", MB_OK | MB_ICONERROR);
        return NULL;
    }
    Logf("CreateWindowW: ok hwnd=%p", hWnd);

    hDC = GetDC(hWnd);
    if (!hDC) {
        LogLastError("GetDC");
        MessageBoxW(NULL, L"GetDC failed.", L"Error", MB_OK | MB_ICONERROR);
        return NULL;
    }

    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | flags;
    pfd.iPixelType = type;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    pf = ChoosePixelFormat(hDC, &pfd);
    if (pf == 0) {
        LogLastError("ChoosePixelFormat");
        MessageBoxW(NULL, L"ChoosePixelFormat failed.", L"Error", MB_OK | MB_ICONERROR);
        return NULL;
    }
    Logf("ChoosePixelFormat: index=%d", pf);

    if (SetPixelFormat(hDC, pf, &pfd) == FALSE) {
        LogLastError("SetPixelFormat");
        MessageBoxW(NULL, L"SetPixelFormat failed.", L"Error", MB_OK | MB_ICONERROR);
        return NULL;
    }

    if (!DescribePixelFormat(hDC, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd)) {
        LogLastError("DescribePixelFormat");
    }
    else {
        Logf("PixelFormat: color=%u depth=%u stencil=%u doublebuffer=%u rgba=%u",
            (unsigned)pfd.cColorBits, (unsigned)pfd.cDepthBits, (unsigned)pfd.cStencilBits,
            (unsigned)((pfd.dwFlags & PFD_DOUBLEBUFFER) != 0), (unsigned)(pfd.iPixelType == PFD_TYPE_RGBA));
    }

    ReleaseDC(hWnd, hDC);
    Logf("CreateOpenGLWindow: end");
    return hWnd;
}

static void InitGL()
{
    Logf("InitGL: start");
    glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    glClearDepth(1.0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    g_ambientLight.enabled = true;
    g_ambientLight.position = Vec3(0.0f, 0.0f, 0.0f);
    g_ambientLight.color = Vec3(1.0f, 1.0f, 1.0f);
    g_ambientLight.intensity = 0.40f;

    g_directionalLight.enabled = true;
    g_directionalLight.position = Vec3(2.0f, 4.0f, 3.0f);
    g_directionalLight.color = Vec3(1.0f, 1.0f, 1.0f);
    g_directionalLight.intensity = 1.0f;

    GLfloat matDiffuse[] = { 0.8f, 0.8f, 0.85f, 1.0f };
    GLfloat matSpecular[] = { 0.1f, 0.1f, 0.1f, 1.0f };
    GLfloat matShine[] = { 24.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matDiffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpecular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, matShine);

    const GLubyte* vendor = glGetString(GL_VENDOR);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* shading = glGetString(GL_SHADING_LANGUAGE_VERSION);
    const GLubyte* ext = glGetString(GL_EXTENSIONS);
    g_glVendor = vendor ? (const char*)vendor : "Unknown";
    g_glRenderer = renderer ? (const char*)renderer : "Unknown";
    g_glVersion = version ? (const char*)version : "Unknown";
    g_glShading = shading ? (const char*)shading : "Fixed-function / not reported";
    g_glExtensions = ext ? (const char*)ext : "";

    Logf("GL Vendor   : %s", g_glVendor.c_str());
    Logf("GL Renderer : %s", g_glRenderer.c_str());
    Logf("GL Version  : %s", g_glVersion.c_str());
    Logf("GL Shading  : %s", g_glShading.c_str());
    LogOpenGLState("InitGL");
    Logf("InitGL: end");
}

static std::wstring GetDefaultModelPath()
{
    return L"model.obj";
}

static void OpenFileDialogAndLoad()
{
    wchar_t fileName[MAX_PATH] = { 0 };
    wcsncpy_s(fileName, MAX_PATH, g_currentObjPath.c_str(), _TRUNCATE);

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Wavefront OBJ (*.obj)\0*.obj\0All files (*.*)\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;
    ofn.lpstrTitle = L"Open OBJ model";

    Logf("OpenFileDialogAndLoad: showing dialog");
    if (GetOpenFileNameW(&ofn)) {
        Logf("OpenFileDialogAndLoad: selected OBJ '%S'", fileName);
        LoadModelAndReport(fileName);
    }
    else {
        DWORD err = CommDlgExtendedError();
        if (err != 0) {
            Logf("GetOpenFileNameW failed: CommDlgExtendedError=0x%08lX", (unsigned long)err);
        }
        else {
            Logf("OpenFileDialogAndLoad: canceled");
        }
    }
}


static void OpenTextureDialogAndApplyToSelected(bool loadFSH)
{
    SceneObject* obj = SelectedObject();
    LogSelectedObjectBinding("OpenTextureDialogAndApplyToSelected: before dialog");

    if (!obj) {
        Logf("OpenTextureDialogAndApplyToSelected: no selected object");
        MessageBoxW(g_hWnd, L"Select a model first, then load a texture set.", L"Texture load", MB_OK | MB_ICONINFORMATION);
        return;
    }

    wchar_t fileName[MAX_PATH] = { 0 };
    if (!obj->sourcePathW.empty()) {
        wcsncpy_s(fileName, MAX_PATH, obj->sourcePathW.c_str(), _TRUNCATE);
    }
    else {
        wcsncpy_s(fileName, MAX_PATH, g_currentObjPath.c_str(), _TRUNCATE);
    }

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;

    std::wstring selectedPath;
    if (loadFSH) {
        ofn.lpstrFilter = L"FSH texture dumps (*.fsh)\0*.fsh\0All files (*.*)\0*.*\0\0";
        ofn.lpstrTitle = L"Load FSH texture set";
    }
    else {
        ofn.lpstrFilter = L"Wavefront MTL (*.mtl)\0*.mtl\0All files (*.*)\0*.*\0\0";
        ofn.lpstrTitle = L"Load MTL texture";
    }

    Logf("OpenTextureDialogAndApplyToSelected: showing %s dialog", loadFSH ? "FSH" : "MTL");
    if (!GetOpenFileNameW(&ofn)) {
        DWORD err = CommDlgExtendedError();
        if (err != 0) {
            Logf("OpenTextureDialogAndApplyToSelected: dialog error=0x%08lX", (unsigned long)err);
        }
        else {
            Logf("OpenTextureDialogAndApplyToSelected: canceled");
        }
        return;
    }

    selectedPath = fileName;
    Logf("OpenTextureDialogAndApplyToSelected: selected '%S'", selectedPath.c_str());
    LogSelectedObjectBinding("OpenTextureDialogAndApplyToSelected: target model");

    bool applied = false;
    std::string err;

    if (loadFSH) {
        std::vector<std::wstring> candidates;
        if (!ParseFSHTexturePath(selectedPath, candidates, err)) {
            Logf("FSH parse failed: %s", err.c_str());
            MessageBoxA(g_hWnd, err.c_str(), "FSH parse failed", MB_OK | MB_ICONERROR);
            return;
        }

        std::wstring diffusePath;
        std::wstring alphaPath;
        for (size_t i = 0; i < candidates.size(); ++i) {
            std::wstring base = GetBaseName(candidates[i]);
            std::string baseA = ToLowerASCII(WStringToUtf8(base));
            if (alphaPath.empty() && (baseA.find("alpha") != std::string::npos || baseA.find("-a.") != std::string::npos || baseA.find("_a.") != std::string::npos)) {
                alphaPath = candidates[i];
            }
            else if (diffusePath.empty()) {
                diffusePath = candidates[i];
            }
        }

        if (diffusePath.empty() && !candidates.empty()) {
            diffusePath = candidates[0];
        }

        if (diffusePath.empty()) {
            Logf("FSH load: no usable bitmap candidate");
            MessageBoxW(g_hWnd, L"FSH file did not contain a usable BMP reference.", L"FSH load", MB_OK | MB_ICONERROR);
            return;
        }

        TextureImage diffuseImg;
        if (!LoadBMPImageFromFile(diffusePath, diffuseImg, err)) {
            Logf("FSH load: diffuse failed '%S' (%s)", diffusePath.c_str(), err.c_str());
            MessageBoxA(g_hWnd, err.c_str(), "FSH texture load failed", MB_OK | MB_ICONERROR);
            return;
        }

        if (!alphaPath.empty()) {
            TextureImage alphaImg;
            std::string alphaErr;
            if (LoadBMPImageFromFile(alphaPath, alphaImg, alphaErr)) {
                if (alphaImg.width == diffuseImg.width && alphaImg.height == diffuseImg.height) {
                    for (int y = 0; y < diffuseImg.height; ++y) {
                        for (int x = 0; x < diffuseImg.width; ++x) {
                            size_t i = ((size_t)y * (size_t)diffuseImg.width + (size_t)x) * 4;
                            diffuseImg.rgba[i + 3] = alphaImg.rgba[i + 0];
                        }
                    }
                    Logf("FSH alpha combined from '%S'", alphaPath.c_str());
                }
                else {
                    Logf("FSH alpha ignored due to size mismatch: diffuse=%dx%d alpha=%dx%d",
                        diffuseImg.width, diffuseImg.height, alphaImg.width, alphaImg.height);
                }
            }
            else {
                Logf("FSH alpha file could not be loaded: '%S' (%s)", alphaPath.c_str(), alphaErr.c_str());
            }
        }

        GLuint tex = CreateGLTextureFromImage(diffuseImg);
        if (!tex) {
            Logf("FSH load: texture creation failed for '%S'", diffusePath.c_str());
            MessageBoxW(g_hWnd, L"Failed to create the OpenGL texture.", L"FSH load", MB_OK | MB_ICONERROR);
            return;
        }

        ReleaseSceneTexture(*obj);
        obj->textureId = tex;
        obj->hasTexture = true;
        obj->textureSourceW = selectedPath;
        applied = true;

        Logf("FSH applied to '%S': diffuse='%S' alpha='%S' texture=%u candidates=%u",
            obj->displayNameW.c_str(), diffusePath.c_str(), alphaPath.empty() ? L"(none)" : alphaPath.c_str(), (unsigned)tex, (unsigned)candidates.size());
        LogSelectedObjectBinding("FSH applied");
    }
    else {
        std::wstring texturePath;
        if (!ParseMTLTexturePath(selectedPath, texturePath, err)) {
            Logf("MTL parse failed: %s", err.c_str());
            MessageBoxA(g_hWnd, err.c_str(), "MTL parse failed", MB_OK | MB_ICONERROR);
            return;
        }

        if (!ApplyTextureToSceneObjectFromImagePath(*obj, texturePath, "MTL")) {
            MessageBoxW(g_hWnd, L"MTL referenced texture could not be loaded.", L"MTL load", MB_OK | MB_ICONERROR);
            return;
        }

        obj->textureSourceW = texturePath;
        applied = true;
        Logf("MTL applied to '%S': '%S'", obj->displayNameW.c_str(), texturePath.c_str());
        LogSelectedObjectBinding("MTL applied");
    }

    if (applied) {
        UpdateWindowTitle();
        InvalidateRect(g_hWnd, NULL, FALSE);
    }
}

// ------------------------------------------------------------
// WinMain
// ------------------------------------------------------------
int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int nCmdShow)
{
    InitLogging();
    Logf("Application start");

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {
        g_currentObjPath = argv[1];
        Logf("Command line model path: '%S'", g_currentObjPath.c_str());
    }
    else {
        g_currentObjPath = GetDefaultModelPath();
        Logf("Default model path: '%S'", g_currentObjPath.c_str());
    }
    if (argv) LocalFree(argv);

    HMENU hMenu = CreateAppMenu();
    g_mainMenu = hMenu;
    if (!hMenu) {
        LogLastError("CreateAppMenu");
    }

    g_hWnd = CreateOpenGLWindow(L"OBJ scene demo", 80, 80, 1280, 720, PFD_TYPE_RGBA, 0, hMenu);
    if (g_hWnd == NULL) {
        Logf("Fatal: window creation failed");
        CloseLogging();
        return 1;
    }

    g_hDC = GetDC(g_hWnd);
    if (!g_hDC) {
        LogLastError("GetDC(main)");
        CloseLogging();
        return 1;
    }

    g_hRC = wglCreateContext(g_hDC);
    if (!g_hRC) {
        LogLastError("wglCreateContext");
        MessageBoxW(g_hWnd, L"Failed to create the OpenGL context.", L"Error", MB_OK | MB_ICONERROR);
        CloseLogging();
        return 1;
    }
    Logf("wglCreateContext: ok rc=%p", g_hRC);

    if (!wglMakeCurrent(g_hDC, g_hRC)) {
        LogLastError("wglMakeCurrent");
        MessageBoxW(g_hWnd, L"Failed to activate the OpenGL context.", L"Error", MB_OK | MB_ICONERROR);
        CloseLogging();
        return 1;
    }
    Logf("wglMakeCurrent: ok");

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    InitGL();
    InitFont(g_hDC);

    LoadModelAndReport(g_currentObjPath.c_str());
    UpdateWindowTitle();

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));

    while (msg.message != WM_QUIT) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (msg.message != WM_QUIT) {
            RenderFrame();
        }
    }

    Logf("Shutdown start");

    for (size_t i = 0; i < g_sceneObjects.size(); ++i) {
        ReleaseSceneTexture(g_sceneObjects[i]);
    }

    if (g_fontBase != 0) {
        glDeleteLists(g_fontBase, 96);
        g_fontBase = 0;
    }

    wglMakeCurrent(NULL, NULL);
    if (g_hRC) {
        wglDeleteContext(g_hRC);
        g_hRC = NULL;
    }
    if (g_hWnd && g_hDC) {
        ReleaseDC(g_hWnd, g_hDC);
        g_hDC = NULL;
    }
    if (g_hWnd) {
        DestroyWindow(g_hWnd);
        g_hWnd = NULL;
    }

    if (hMenu) DestroyMenu(hMenu);
    CloseLogging();
    return (int)msg.wParam;
}
