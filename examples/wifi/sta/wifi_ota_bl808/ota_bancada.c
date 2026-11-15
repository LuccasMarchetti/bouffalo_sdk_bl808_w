/*
 * wifi_ota_bl808: shell commands for the OTA test on the bench.
 *
 *  ota_versao   prints the label this image was built with (OTA_VERSION)
 *  (bancada_isp, which ends the test, is in the m1s_dock board file)
 */
#include <stdio.h>
#include "shell.h"

static int cmd_ota_versao(int argc, char **argv)
{
    printf("OTA_VERSION %s\r\n", OTA_VERSION);
    return 0;
}

SHELL_CMD_EXPORT_ALIAS(cmd_ota_versao, ota_versao, print the image label);
