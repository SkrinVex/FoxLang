#include "foxlang/Project.h"
#include "foxlang/Lexer.h"
#include "foxlang/Parser.h"
#include "foxlang/Platform.h"
#include <algorithm>
#include <deque>
#include <set>

namespace foxlang {
namespace fs = std::filesystem;

std::string canonicalPath(const std::string& path) {
    std::error_code ec;
    auto canonical = fs::weakly_canonical(platform::pathFromUtf8(path), ec);
    return ec ? path : platform::pathToUtf8(canonical);
}

std::string OverlaySources::resolve(const ModuleImport& request, const std::string& from) const {
    return base->resolve(request, from);
}

std::string OverlaySources::read(const std::string& identity) const {
    if (const std::string* text = find(identity)) return *text;
    return base->read(identity);
}

void OverlaySources::set(const std::string& path, std::string text) { buffers[canonicalPath(path)] = std::move(text); }
void OverlaySources::erase(const std::string& path) { buffers.erase(canonicalPath(path)); }

const std::string* OverlaySources::find(const std::string& identity) const {
    auto found = buffers.find(identity);
    return found == buffers.end() ? nullptr : &found->second;
}

ProjectIndex::ProjectIndex(std::shared_ptr<const OverlaySources> provider) : sources(std::move(provider)) {}

// .fox files below the root, skipping hidden, build and dependency directories.
std::vector<std::string> ProjectIndex::scan(const std::string& file) const {
    std::vector<std::string> files;
    fs::path root = rootDir.empty() ? platform::pathFromUtf8(file).parent_path() : platform::pathFromUtf8(rootDir);
    std::error_code ec;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
    for (; !ec && it != end && files.size() < maxFiles; it.increment(ec)) {
        const auto& path = it->path();
        std::string name = platform::pathToUtf8(path.filename());
        if (it->is_directory(ec)) {
            if (it.depth() >= 8 || (!name.empty() && name[0] == '.') || name == "node_modules" || name == "dist" ||
                name.rfind("build", 0) == 0) {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (path.extension() == ".fox" && it->is_regular_file(ec)) files.push_back(canonicalPath(platform::pathToUtf8(path)));
    }
    return files;
}

const std::vector<ModuleImport>& ProjectIndex::importsOf(const std::string& path) {
    static const std::vector<ModuleImport> none;
    const std::string* buffer = sources->find(path);
    std::error_code ec;
    Entry probe;
    if (!buffer) {
        probe.modified = fs::last_write_time(platform::pathFromUtf8(path), ec);
        if (ec) return none;
        probe.size = fs::file_size(platform::pathFromUtf8(path), ec);
        if (ec) return none;
        auto cached = cache.find(path);
        if (cached != cache.end() && cached->second.modified == probe.modified && cached->second.size == probe.size)
            return cached->second.imports;
    }
    try {
        Lexer lexer(sources->read(path), true);
        Parser parser(lexer.tokenize(), path);
        std::vector<Diagnostic> ignored;
        parser.parseProgramWithDiagnostics(ignored);
        probe.imports = parser.getImports();
    } catch (const std::exception&) {
        probe.imports.clear();
    }
    // An open buffer changes with every keystroke; it is parsed each time, not cached.
    auto& entry = cache[path];
    entry = std::move(probe);
    if (buffer) entry.size = static_cast<std::uintmax_t>(-1);
    return entry.imports;
}

std::vector<std::string> ProjectIndex::peers(const std::string& file) {
    std::string start = canonicalPath(file);
    std::string scope = rootDir.empty() ? platform::pathToUtf8(platform::pathFromUtf8(start).parent_path()) : rootDir;
    auto now = std::chrono::steady_clock::now();
    if (scope != scannedFor || now - scannedAt > std::chrono::seconds(2)) {
        scanned = scan(start);
        scannedFor = scope;
        scannedAt = now;
    }
    std::vector<std::string> files = scanned;
    if (std::find(files.begin(), files.end(), start) == files.end()) files.push_back(start);

    std::map<std::string, std::set<std::string>> imports, importers;
    std::deque<std::string> pending(files.begin(), files.end());
    std::set<std::string> seen(files.begin(), files.end());
    while (!pending.empty()) {
        std::string path = pending.front();
        pending.pop_front();
        for (const auto& request : importsOf(path)) {
            std::string target;
            try {
                target = sources->resolve(request, path);
            } catch (const std::exception&) {
                continue;
            }
            if (target.empty() || target[0] == '@') continue; // the embedded stdlib is not a project file
            imports[path].insert(target);
            importers[target].insert(path);
            // A module outside the root still belongs to the program; follow its imports too.
            if (seen.insert(target).second && seen.size() <= maxFiles) pending.push_back(target);
        }
    }

    auto reach = [](const std::map<std::string, std::set<std::string>>& edges, std::set<std::string>& found,
                    std::deque<std::string> queue) {
        while (!queue.empty()) {
            std::string path = queue.front();
            queue.pop_front();
            auto next = edges.find(path);
            if (next == edges.end()) continue;
            for (const auto& target : next->second)
                if (found.insert(target).second) queue.push_back(target);
        }
    };
    // The programs this file is part of are the files that import it, directly or not.
    // Everything those programs and this file import runs alongside it. Two programs
    // that share a library are not peers of each other.
    std::set<std::string> ancestors;
    reach(importers, ancestors, {start});
    std::set<std::string> group(ancestors);
    std::deque<std::string> roots(ancestors.begin(), ancestors.end());
    roots.push_back(start);
    reach(imports, group, roots);
    group.erase(start);
    return {group.begin(), group.end()};
}

} // namespace foxlang
