#include "geom-core/cad/ShapeRegistry.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <random>

namespace madfam::geom::cad {

// ===========================================================================
// ShapeRegistry Implementation
// ===========================================================================

ShapeRegistry& ShapeRegistry::instance() {
    static ShapeRegistry instance;
    return instance;
}

ShapeRegistry::ShapeRegistry() = default;
ShapeRegistry::~ShapeRegistry() = default;

std::string ShapeRegistry::generateId() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Generate ID: shape_XXXXXX where X is hex
    std::stringstream ss;
    ss << "shape_" << std::hex << std::setw(6) << std::setfill('0') << nextId_++;
    return ss.str();
}

std::string ShapeRegistry::registerShape(std::unique_ptr<InternalShape> shape, ShapeType type) {
    if (!shape) {
        return "";
    }
    
    std::string id = generateId();
    
    ShapeHandle handle;
    handle.id = id;
    handle.type = type;
    handle.bbox = shape->getBoundingBox();
    handle.hash = shape->computeHash();
    
    ShapeEntry entry;
    entry.shape = std::move(shape);
    entry.handle = handle;
    entry.lastAccess = std::chrono::steady_clock::now();
    entry.estimatedBytes = entry.shape->getEstimatedMemoryBytes();
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shapes_[id] = std::move(entry);
    }
    
    // Notify callbacks
    for (const auto& cb : createdCallbacks_) {
        cb(handle);
    }
    
    return id;
}

bool ShapeRegistry::hasShape(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return shapes_.find(id) != shapes_.end();
}

InternalShape* ShapeRegistry::getShape(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = shapes_.find(id);
    if (it == shapes_.end()) {
        return nullptr;
    }
    
    // Update access time for LRU
    it->second.lastAccess = std::chrono::steady_clock::now();
    return it->second.shape.get();
}

const InternalShape* ShapeRegistry::getShape(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = shapes_.find(id);
    if (it == shapes_.end()) {
        return nullptr;
    }
    return it->second.shape.get();
}

ShapeHandle ShapeRegistry::getHandle(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = shapes_.find(id);
    if (it == shapes_.end()) {
        return ShapeHandle{};  // Invalid handle
    }
    return it->second.handle;
}

bool ShapeRegistry::disposeShape(const std::string& id) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = shapes_.find(id);
        if (it == shapes_.end()) {
            return false;
        }
        shapes_.erase(it);
    }
    
    // Invalidate any cached operations using this shape
    invalidateCacheFor(id);
    
    // Notify callbacks
    for (const auto& cb : disposedCallbacks_) {
        cb(id);
    }
    
    return true;
}

void ShapeRegistry::disposeAll() {
    std::vector<std::string> ids;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& pair : shapes_) {
            ids.push_back(pair.first);
        }
        shapes_.clear();
        operationCache_.clear();
    }
    
    // Notify callbacks
    for (const auto& id : ids) {
        for (const auto& cb : disposedCallbacks_) {
            cb(id);
        }
    }
}

std::vector<ShapeHandle> ShapeRegistry::getAllHandles() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ShapeHandle> handles;
    handles.reserve(shapes_.size());
    for (const auto& pair : shapes_) {
        handles.push_back(pair.second.handle);
    }
    return handles;
}

std::vector<std::string> ShapeRegistry::getShapeIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> ids;
    ids.reserve(shapes_.size());
    for (const auto& pair : shapes_) {
        ids.push_back(pair.first);
    }
    return ids;
}

size_t ShapeRegistry::getShapeCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return shapes_.size();
}

size_t ShapeRegistry::getEstimatedMemoryBytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t total = 0;
    for (const auto& pair : shapes_) {
        total += pair.second.estimatedBytes;
    }
    return total;
}

void ShapeRegistry::setMemoryLimit(size_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    memoryLimit_ = bytes;
}

void ShapeRegistry::evictLRU(size_t targetBytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t currentBytes = 0;
    for (const auto& pair : shapes_) {
        currentBytes += pair.second.estimatedBytes;
    }
    
    if (currentBytes <= targetBytes) {
        return;
    }
    
    // Sort shapes by last access time
    std::vector<std::pair<std::string, std::chrono::steady_clock::time_point>> sorted;
    for (const auto& pair : shapes_) {
        sorted.emplace_back(pair.first, pair.second.lastAccess);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });
    
    // Evict oldest shapes until under target
    for (const auto& item : sorted) {
        if (currentBytes <= targetBytes) {
            break;
        }
        
        auto it = shapes_.find(item.first);
        if (it != shapes_.end()) {
            currentBytes -= it->second.estimatedBytes;
            shapes_.erase(it);
        }
    }
}

void ShapeRegistry::cacheResult(const std::string& operationKey, const std::string& resultShapeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    operationCache_[operationKey] = resultShapeId;
}

std::optional<std::string> ShapeRegistry::getCachedResult(const std::string& operationKey) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = operationCache_.find(operationKey);
    if (it == operationCache_.end()) {
        cacheMisses_++;
        return std::nullopt;
    }
    
    // Verify the cached shape still exists
    if (shapes_.find(it->second) == shapes_.end()) {
        cacheMisses_++;
        return std::nullopt;
    }
    
    cacheHits_++;
    return it->second;
}

void ShapeRegistry::invalidateCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    operationCache_.clear();
}

void ShapeRegistry::invalidateCacheFor(const std::string& shapeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Remove any cache entries that reference this shape
    for (auto it = operationCache_.begin(); it != operationCache_.end();) {
        if (it->second == shapeId || it->first.find(shapeId) != std::string::npos) {
            it = operationCache_.erase(it);
        } else {
            ++it;
        }
    }
}

ShapeRegistry::Stats ShapeRegistry::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Stats stats;
    stats.totalShapes = shapes_.size();
    stats.totalMemoryBytes = 0;
    for (const auto& pair : shapes_) {
        stats.totalMemoryBytes += pair.second.estimatedBytes;
    }
    stats.cacheHits = cacheHits_;
    stats.cacheMisses = cacheMisses_;
    
    if (!operationDurations_.empty()) {
        double sum = 0;
        for (double d : operationDurations_) {
            sum += d;
        }
        stats.averageOperationMs = sum / operationDurations_.size();
    } else {
        stats.averageOperationMs = 0;
    }
    
    return stats;
}

void ShapeRegistry::resetStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    cacheHits_ = 0;
    cacheMisses_ = 0;
    operationDurations_.clear();
}

void ShapeRegistry::onShapeCreated(ShapeCreatedCallback cb) {
    createdCallbacks_.push_back(std::move(cb));
}

void ShapeRegistry::onShapeDisposed(ShapeDisposedCallback cb) {
    disposedCallbacks_.push_back(std::move(cb));
}

void ShapeRegistry::recordOperation(double durationMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    operationDurations_.push_back(durationMs);
    
    // Keep only last 1000 operations for rolling average
    if (operationDurations_.size() > 1000) {
        operationDurations_.erase(operationDurations_.begin());
    }
}

// ===========================================================================
// ShapeGuard Implementation
// ===========================================================================

ShapeGuard::ShapeGuard(const std::string& shapeId) 
    : shapeId_(shapeId), shouldDispose_(true) {}

ShapeGuard::~ShapeGuard() {
    if (shouldDispose_ && !shapeId_.empty()) {
        ShapeRegistry::instance().disposeShape(shapeId_);
    }
}

ShapeGuard::ShapeGuard(ShapeGuard&& other) noexcept
    : shapeId_(std::move(other.shapeId_))
    , shouldDispose_(other.shouldDispose_) {
    other.shouldDispose_ = false;
}

ShapeGuard& ShapeGuard::operator=(ShapeGuard&& other) noexcept {
    if (this != &other) {
        if (shouldDispose_ && !shapeId_.empty()) {
            ShapeRegistry::instance().disposeShape(shapeId_);
        }
        shapeId_ = std::move(other.shapeId_);
        shouldDispose_ = other.shouldDispose_;
        other.shouldDispose_ = false;
    }
    return *this;
}

void ShapeGuard::release() {
    shouldDispose_ = false;
}

} // namespace madfam::geom::cad
