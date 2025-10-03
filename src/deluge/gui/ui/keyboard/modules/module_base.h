#pragma once

#include "definitions.h"
#include "gui/ui/keyboard/layout.h"

namespace deluge::gui::ui::keyboard::modules {

/// @brief Base interface for all keyboard modules
/// @details Modules extend keyboard layouts with additional functionality like
///          audio visualization, timing displays, custom controls, etc.
class KeyboardModule {
public:
	KeyboardModule() = default;
	virtual ~KeyboardModule() = default;

	/// @brief Initialize the module
	/// @return true if initialization successful, false otherwise
	virtual bool init() = 0;

	/// @brief Update module state (called every frame)
	/// @param deltaTime Time since last update in milliseconds
	virtual void update(uint32_t deltaTime) = 0;

	/// @brief Render module visuals on the keyboard grid
	/// @param image LED color array to modify
	/// @param occupancyMask Occupancy mask for LED updates
	virtual void render(RGB image[][kDisplayWidth + kSideBarWidth],
	                    uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) = 0;

	/// @brief Handle pad press events
	/// @param x X coordinate of pressed pad
	/// @param y Y coordinate of pressed pad
	/// @param velocity Velocity of press (0-127)
	/// @return true if event was handled, false to pass to next module
	virtual bool handlePadPress(int32_t x, int32_t y, int32_t velocity) = 0;

	/// @brief Handle encoder events
	/// @param offset Encoder movement offset
	/// @param isVertical true for vertical encoder, false for horizontal
	/// @return true if event was handled, false to pass to next module
	virtual bool handleEncoder(int32_t offset, bool isVertical) = 0;

	/// @brief Get module metadata
	virtual const char* getName() const = 0;
	virtual const char* getAuthor() const = 0;
	virtual const char* getVersion() const = 0;
	virtual const char* getDescription() const = 0;

	/// @brief Module state management
	virtual bool isEnabled() const { return enabled_; }
	virtual void setEnabled(bool enabled) { enabled_ = enabled; }

	/// @brief Module priority (higher priority modules render on top)
	virtual int32_t getPriority() const { return priority_; }
	virtual void setPriority(int32_t priority) { priority_ = priority; }

	/// @brief Request rendering update (call this when display needs to change)
	void requestRendering();

protected:
	bool enabled_ = true;
	int32_t priority_ = 0;
};

} // namespace deluge::gui::ui::keyboard::modules
