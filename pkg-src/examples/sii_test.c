/*****************************************************************************
 * sii_test.c - 验证 ecrt_master_sii_read / ecrt_master_sii_write
 *
 * 本包新增的 SII/EEPROM 用户态接口测试。
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ecrt.h"

#define SLAVE_POS  0

int main(int argc, char **argv)
{
    ec_master_t *master;
    uint16_t *words;
    uint32_t nwords = 0x200;  /* 512 words = 1024 bytes */
    uint32_t i;
    int ret;

    master = ecrt_request_master(0);
    if (!master) {
        fprintf(stderr, "request_master failed\n");
        return 1;
    }

    words = calloc(nwords, sizeof(uint16_t));
    if (!words) {
        fprintf(stderr, "calloc failed\n");
        return 1;
    }

    printf("=== ecrt_master_sii_read 测试 ===\n");
    ret = ecrt_master_sii_read(master, SLAVE_POS, 0, nwords, words);
    printf("ecrt_master_sii_read() = %d\n", ret);

    if (ret == 0) {
        printf("\n前 16 words:\n");
        for (i = 0; i < 16; i++) {
            printf("  [%3u] 0x%04X\n", i, words[i]);
        }

        /* 解析 SII 头部（word 偏移，与内核 fsm_slave_scan.c 一致） */
        printf("\nSII 头部解析:\n");
        printf("  PDI control   : 0x%04X\n", words[0x00]);
        printf("  ESC config    : 0x%04X\n", words[0x01]);
        printf("  Alias         : 0x%04X\n", words[0x04]);
        printf("  Vendor ID     : 0x%08X\n",
                words[0x08] | ((uint32_t)words[0x09] << 16));
        printf("  Product code  : 0x%08X\n",
                words[0x0A] | ((uint32_t)words[0x0B] << 16));
        printf("  Revision num  : 0x%08X\n",
                words[0x0C] | ((uint32_t)words[0x0D] << 16));
        printf("  Serial number : 0x%08X\n",
                words[0x0E] | ((uint32_t)words[0x0F] << 16));
    }

    /* 写测试：仅在指定 --write 时执行，写回相同内容 */
    if (argc > 1 && strcmp(argv[1], "--write") == 0) {
        printf("\n=== ecrt_master_sii_write 测试（写回相同内容）===\n");
        ret = ecrt_master_sii_write(master, SLAVE_POS, 0, nwords, words);
        printf("ecrt_master_sii_write() = %d\n", ret);
    } else {
        printf("\n(跳过写测试，加 --write 参数执行)\n");
    }

    free(words);
    ecrt_release_master(master);
    return 0;
}
