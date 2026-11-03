// SoundBuddy tests : identity of the tested code, replacing the generated version.h.
// Data directory, registry values and locks then never touch the real app ones.

#pragma once

// macros, as the generated version.h they stand in for
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define APP_PUBLISHER_ORG "SoundBuddyTestsOrg"
#define APP_NAME "SoundBuddyTests"
#define APP_CURRENT_VERSION "0.0.0"
#define DEST_PLATFORM_PRODUCT_NAME "SoundVitrine"
#define APP_UPDATES_REPOSITORY_URL "https://example.invalid/SoundBuddyTests"
// NOLINTEND(cppcoreguidelines-macro-usage)
