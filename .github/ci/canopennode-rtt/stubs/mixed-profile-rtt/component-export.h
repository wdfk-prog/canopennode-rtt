#ifndef MIXED_PROFILE_RTT_COMPONENT_EXPORT_H
#define MIXED_PROFILE_RTT_COMPONENT_EXPORT_H

#define INIT_COMPONENT_EXPORT(function_) int (*const function_##_export)(void) = function_

#endif /* MIXED_PROFILE_RTT_COMPONENT_EXPORT_H */
