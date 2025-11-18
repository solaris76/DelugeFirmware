# API Layer Maintenance & Stability

## The Problem

When Deluge core model changes, the API layer could become out of date:

### Scenario 1: Breaking Changes
```cpp
// Core model changes method signature
// BEFORE:
Clip* Song::createClip(OutputType type, int32_t position);

// AFTER (new parameter added):
Clip* Song::createClip(OutputType type, int32_t position, bool autoSync = true);
```

**Impact**: API layer breaks at compile time ✅ (caught immediately)

### Scenario 2: Method Removed
```cpp
// Core model removes method
// BEFORE:
Drum* Kit::getDrumFromIndex(int32_t index);

// AFTER: Method removed, replaced with iterator pattern
```

**Impact**: API layer breaks at compile time ✅ (caught immediately)

### Scenario 3: Internal Refactoring
```cpp
// Core model changes internal structure but keeps interface
// BEFORE:
Clip* Song::getCurrentClip() { return currentClip; }

// AFTER: Internal refactoring
Clip* Song::getCurrentClip() {
    return sessionView.getActiveClip(); // Different implementation
}
```

**Impact**: API layer continues working ✅ (abstraction hides change)

### Scenario 4: New Features
```cpp
// Core model adds new functionality
// NEW:
void Clip::setLaunchStyle(LaunchStyle style);
```

**Impact**: API layer doesn't expose it ❌ (needs manual update)

## Solutions

### 1. **Abstraction Layer Design** (Recommended)

The API layer should be an **abstraction**, not a direct pass-through:

```cpp
// ❌ BAD: Direct pass-through (breaks if model changes)
Clip* Controller::createClip(OutputType type, int32_t pos) {
    return currentSong->createClip(type, pos); // Direct call
}

// ✅ GOOD: Abstraction layer (adapts to changes)
Clip* Controller::createClip(OutputType type, int32_t pos) {
    // API layer handles the complexity
    // If model changes, only this function needs updating
    if (!currentSong) return nullptr;

    // Could use SessionView, or direct Song method, or new helper
    // Implementation can change without breaking external consumers
    return sessionView.createClipAtIndex(type, pos);
}
```

**Benefits**:
- **Stable interface**: External consumers don't break
- **Adaptable**: Can change implementation without changing interface
- **Single point of change**: Update one function, not all consumers

### 2. **Versioning Strategy**

```cpp
namespace DelugeAPI {
    // API version - increment when breaking changes occur
    constexpr uint32_t API_VERSION = 1;

    struct APIInfo {
        uint32_t version;
        const char* description;
    };

    APIInfo getAPIInfo() {
        return {API_VERSION, "Deluge API v1.0"};
    }
}
```

**Benefits**:
- External tools can check API version
- Breaking changes can be versioned
- Deprecation warnings possible

### 3. **Compile-Time Safety**

The API layer will **fail to compile** if model changes break it:

```cpp
// If Song::createClip signature changes, this won't compile
Clip* Controller::createClip(OutputType type, int32_t pos) {
    return currentSong->createClip(type, pos); // Compiler error if signature changed
}
```

**Benefits**:
- **Immediate feedback**: Can't ship broken code
- **Type safety**: Compiler catches mismatches
- **Refactoring safety**: IDE can update all call sites

### 4. **Testing Strategy**

```cpp
// Unit tests catch regressions
TEST(DelugeAPI, CreateClip) {
    // Setup
    Song* song = createTestSong();

    // Test
    Clip* clip = DelugeAPI::Controller::createClip(OutputType::SYNTH, 0);

    // Verify
    ASSERT_NE(clip, nullptr);
    ASSERT_EQ(clip->output->type, OutputType::SYNTH);
}
```

**Benefits**:
- Catches runtime issues
- Documents expected behavior
- Prevents regressions

### 5. **Deprecation Warnings**

```cpp
// Mark deprecated methods
[[deprecated("Use createClipWithOptions() instead")]]
Clip* Controller::createClip(OutputType type, int32_t pos);

// New method with more options
Clip* Controller::createClipWithOptions(const ClipOptions& options);
```

**Benefits**:
- Gradual migration path
- Clear upgrade path
- Backward compatibility

## Maintenance Workflow

### When Core Model Changes:

1. **Compile-time detection** ✅
   - Build fails if API layer breaks
   - Fix immediately

2. **Update API layer** (if needed)
   ```cpp
   // OLD:
   Clip* Controller::createClip(OutputType type, int32_t pos) {
       return currentSong->createClip(type, pos);
   }

   // NEW (adapts to model change):
   Clip* Controller::createClip(OutputType type, int32_t pos) {
       // Model now requires autoSync parameter
       return currentSong->createClip(type, pos, true); // Default value
   }
   ```

3. **Update tests** (if behavior changed)
   ```cpp
   TEST(DelugeAPI, CreateClip) {
       // Test new behavior
   }
   ```

4. **Version bump** (if breaking change)
   ```cpp
   constexpr uint32_t API_VERSION = 2; // Breaking change
   ```

## Comparison: API Layer vs Direct SysEx

### Direct SysEx (Current Approach)
```cpp
// If model changes, EVERY handler needs updating
void createClip(MIDICable& cable, JsonDeserializer& reader) {
    // 50 lines of manual navigation
    Clip* clip = sessionView.createClipAtIndex(type, pos);
    // If createClipAtIndex changes, this breaks
}
```

**Maintenance**: Update every handler individually ❌

### API Layer (Proposed)
```cpp
// If model changes, update ONE function
Clip* Controller::createClip(OutputType type, int32_t pos) {
    return sessionView.createClipAtIndex(type, pos);
    // Update here, all handlers continue working
}

// All handlers use stable API
void createClip(MIDICable& cable, JsonDeserializer& reader) {
    Clip* clip = DelugeAPI::Controller::createClip(type, pos);
    // No changes needed here
}
```

**Maintenance**: Update one function, all handlers benefit ✅

## Best Practices

### 1. **Hide Implementation Details**
```cpp
// ✅ GOOD: Hides how clips are created
Clip* Controller::createClip(OutputType type, int32_t pos);

// ❌ BAD: Exposes internal structure
Clip* Controller::createClipViaSessionView(OutputType type, int32_t pos);
```

### 2. **Provide Stable Abstractions**
```cpp
// ✅ GOOD: Stable interface
Result Controller::setParameter(Clip* clip, const char* name, int32_t value);

// ❌ BAD: Exposes ModelStack complexity
ModelStackWithAutoParam* Controller::getParamStack(Clip* clip, const char* name);
// External code shouldn't need to know about ModelStack
```

### 3. **Document Breaking Changes**
```cpp
/**
 * Creates a new clip.
 *
 * @param type The output type (SYNTH, KIT, MIDI, etc.)
 * @param pos The insertion position
 * @return The created clip, or nullptr on failure
 *
 * @note API v2: Now requires valid Song context
 * @deprecated Use createClipWithOptions() for more control
 */
Clip* Controller::createClip(OutputType type, int32_t pos);
```

## Real-World Example

### Scenario: Model Refactoring

**Before**:
```cpp
// Core model
Clip* SessionView::createClipAtIndex(OutputType type, int32_t index);

// API layer
Clip* Controller::createClip(OutputType type, int32_t pos) {
    return sessionView.createClipAtIndex(type, pos);
}
```

**After** (model refactored):
```cpp
// Core model changed
Clip* Song::createSessionClip(OutputType type, int32_t index, SessionView* view);

// API layer adapts (ONE change)
Clip* Controller::createClip(OutputType type, int32_t pos) {
    // Updated to use new model method
    return currentSong->createSessionClip(type, pos, &sessionView);
}

// All SysEx handlers continue working without changes ✅
```

## Conclusion

**Yes, the API layer can become out of date**, but:

1. ✅ **Compile-time safety**: Breaks are caught immediately
2. ✅ **Single point of change**: Update one function, not all handlers
3. ✅ **Abstraction**: Hides implementation details
4. ✅ **Testing**: Catches regressions
5. ✅ **Versioning**: Manages breaking changes

**The API layer is MORE maintainable than direct SysEx** because:
- Changes are localized to one function
- External consumers (SysEx handlers, UI) don't break
- Compiler catches issues immediately
- Tests document expected behavior

**Trade-off**: Small maintenance overhead for much better maintainability.

