#ifndef CIA401_RTT_HOST_STUB_FINSH_H
#define CIA401_RTT_HOST_STUB_FINSH_H

int rt_kprintf(const char *format, ...);

#define INIT_COMPONENT_EXPORT(function_) \
    static int (*const function_##_component_export)(void) __attribute__((unused)) = function_
#define MSH_CMD_EXPORT(command_, description_) \
    static int (*const command_##_msh_export)(int, char **) __attribute__((unused)) = command_

#endif /* CIA401_RTT_HOST_STUB_FINSH_H */
