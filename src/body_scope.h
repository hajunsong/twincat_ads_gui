#pragma once

#include <cstddef>

/** GUI body scope: which module indices (M0…M30) are active in the UI. */
enum class BodyScope {
	WholeBody = 0,
	UpperBody = 1,
	UpperBodyMini = 2,
	LowerBody = 3,
	Module = 4,
};

struct BodyScopeRange {
	int firstModule;
	int lastModule;
	int moduleCount;
};

inline BodyScopeRange bodyScopeRange(BodyScope scope) {
	switch (scope) {
		case BodyScope::UpperBody:
			return {0, 15, 16};
		case BodyScope::UpperBodyMini:
			return {2, 8, 7};
		case BodyScope::LowerBody:
			return {16, 30, 15};
		case BodyScope::Module:
			return {0, 0, 1};
		case BodyScope::WholeBody:
		default:
			return {0, 30, 31};
	}
}

inline bool moduleInBodyScope(int moduleId, BodyScope scope) {
	const BodyScopeRange r = bodyScopeRange(scope);
	return moduleId >= r.firstModule && moduleId <= r.lastModule;
}

inline int localSlotForModule(int moduleId, BodyScope scope) {
	return moduleId - bodyScopeRange(scope).firstModule;
}

inline int realModuleForLocalSlot(int localSlot, BodyScope scope) {
	return bodyScopeRange(scope).firstModule + localSlot;
}

inline int displayIndexForRealModule(int realModule, BodyScope scope) {
	return localSlotForModule(realModule, scope);
}

inline int realModuleFromDisplayIndex(int displayIndex, BodyScope scope) {
	return realModuleForLocalSlot(displayIndex, scope);
}

inline bool localSlotForModuleChecked(int moduleId, BodyScope scope, int *outLocalSlot) {
	if (!moduleInBodyScope(moduleId, scope)) {
		return false;
	}
	const BodyScopeRange range = bodyScopeRange(scope);
	const int localSlot = moduleId - range.firstModule;
	if (localSlot < 0 || localSlot >= range.moduleCount) {
		return false;
	}
	if (outLocalSlot != nullptr) {
		*outLocalSlot = localSlot;
	}
	return true;
}
