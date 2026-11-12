/*
 * wifi_ota_bl808: shell commands for the OTA test on the bench.
 *
 *  ota_versao   prints the label this image was built with (OTA_VERSION)
 *  bancada_isp  erases the boot header (boot2) and resets: with no valid
 *               image the boot ROM waits for the flash tool on UART0
 */
#include <stdio.h>
#include "shell.h"
#include "bflb_flash.h"
#include "bl808_glb.h"

static int cmd_ota_versao(int argc, char **argv)
{
    printf("OTA_VERSION %s\r\n", OTA_VERSION);
    return 0;
}

static int cmd_bancada_isp(int argc, char **argv)
{
    int ret = bflb_flash_erase(0, 4096);

    printf("BANCADA: cabecalho do boot2 apagado (ret %d), reiniciando\r\n", ret);
    GLB_SW_POR_Reset();
    return 0;
}

SHELL_CMD_EXPORT_ALIAS(cmd_ota_versao, ota_versao, print the image label);
SHELL_CMD_EXPORT_ALIAS(cmd_bancada_isp, bancada_isp, erase the boot header and reset);
