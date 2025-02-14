// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#    if defined(WINAPI_FAMILY) && !WINAPI_PARTITION_DESKTOP
#        error "Only WINAPI_PARTITION_DESKTOP is supported, because of LoadLibrary[A|W]"
#    endif
#elif defined(__linux) || defined(__APPLE__)
#    include <dlfcn.h>
#endif

#include <sstream>

#include "backend.hpp"
#include "backend_manager.hpp"
#include "dynamic/dynamic_backend.hpp"
#include "ngraph/file_util.hpp"
#include "ngraph/util.hpp"

using namespace std;
using namespace ngraph;

std::mutex runtime::Backend::m_mtx;
std::string runtime::Backend::s_backend_shared_library_search_directory;

// This finds the full path of the containing shared library
static string find_my_pathname() {
#ifdef _WIN32
    HMODULE hModule = GetModuleHandleW(SHARED_LIB_PREFIX L"ngraph" SHARED_LIB_SUFFIX);
    WCHAR wpath[MAX_PATH];
    GetModuleFileNameW(hModule, wpath, MAX_PATH);
    wstring ws(wpath);
    string path(ws.begin(), ws.end());
    replace(path.begin(), path.end(), '\\', '/');
    NGRAPH_SUPPRESS_DEPRECATED_START
    path = file_util::get_directory(path);
    NGRAPH_SUPPRESS_DEPRECATED_END
    path += "/";
    return path;
#elif defined(__linux) || defined(__APPLE__)
    Dl_info dl_info;
    dladdr(reinterpret_cast<void*>(ngraph::to_lower), &dl_info);
    return dl_info.dli_fname;
#else
#    error "Unsupported OS"
#endif
}

runtime::Backend::~Backend() {}

std::shared_ptr<runtime::Backend> runtime::Backend::create(const string& t, bool must_support_dynamic) {
    // Rewrite backend name BACKEND_OPTION to BACKEND:OPTION
    string type = t;
    auto pos = type.find('_');
    if (pos != string::npos) {
        type = type.replace(pos, 1, ":");
    }

    auto inner_backend = BackendManager::create_backend(type);

    if (!must_support_dynamic || inner_backend->supports_dynamic_tensors()) {
        return inner_backend;
    } else {
        return make_shared<runtime::dynamic::DynamicBackend>(inner_backend);
    }
    return inner_backend;
}

vector<string> runtime::Backend::get_registered_devices() {
    return BackendManager::get_registered_backends();
}

std::shared_ptr<ngraph::runtime::Tensor> runtime::Backend::create_dynamic_tensor(
    const ngraph::element::Type& /* element_type */,
    const PartialShape& /* shape */) {
    throw std::invalid_argument("This backend does not support dynamic tensors");
}

bool runtime::Backend::is_supported(const Node& /* node */) const {
    // The default behavior is that a backend does not support any ops. If this is not the case
    // then override this method and enhance.
    return false;
}

std::shared_ptr<runtime::Executable> runtime::Backend::load(istream& /* input_stream */) {
    throw runtime_error("load operation unimplemented.");
}

void runtime::Backend::set_backend_shared_library_search_directory(const string& path) {
    std::lock_guard<std::mutex> lock(runtime::Backend::m_mtx);
    s_backend_shared_library_search_directory = path;
}

const string& runtime::Backend::get_backend_shared_library_search_directory() {
    if (s_backend_shared_library_search_directory.empty()) {
        s_backend_shared_library_search_directory = find_my_pathname();
    }
    return s_backend_shared_library_search_directory;
}

bool runtime::Backend::set_config(const map<string, string>& /* config */, string& error) {
    error = "set_config not supported";
    return false;
}
