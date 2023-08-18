#ifndef _FLASH_H
#define _FLASH_H

#include <stdint.h>
#include <stdbool.h>

#include "spi.h"
#include "lfs.h"
#include "equipment.h"

typedef struct
{
    uint16_t    PgSize;                                                         // размер одной страницы памяти в байтах
    uint32_t    Pages;                                                          // общее количество страниц памяти на м/сх
    uint16_t    ErasableSize;                                                   // минимальный размер стираемого блока (только для LittleFS)
    uint16_t    NumOfErasable;                                                  // общее количество стираемых блоков   (только для LittleFS)
    uint8_t	    Shift;                                                          // число фиктивных битов сдвига для вычисления абсолютного адреса
    uint8_t     Id;                                                             // внутренний уникальный номер-идентификатор м/сх (1-63: для AT45DBXXX; 64-255: для W25QXX)
    bool        Busy;                                                           // флаг занятости м/сх памяти: true = м/сх выполняет команду, false = м/сх готова для выполнения команды
} FLASH_t;

// -------------- общие определения для для м/сх FLASH-памяти серий AT45DBXX и W25QXXX --------------------
#define FLASH_GET_JEDEC_ID      0x9F                                            // чтение идентификатора устройства. MF(7:0), ID(15:8), ID(7:0)
#define FLASH_PWRDOWN           0xB9                                            // режим сна с последующим снижением потребления энергии
#define FLASH_RESUME            0xAB                                            // возобновление работы после сна
#define FLASH_CHIP_ERASE        0xC7                                            // стирание чипа
#define DUMMY_BYTE              0xA5                                            // прикольное число-палиндром


// -------------- определения для м/сх FLASH-памяти серии AT45DBXX --------------------
// команды чтения
#define AT45_RDMN               0xD2                                            // чтение основной страницы памяти
#define AT45_RDARRY             0xE8                                            // непрерывное чтение массива (унаследованная команда) 
#define AT45_RDARRAYLF          0x03                                            // непрерывное чтение массива (низкая частота) 
#define AT45_RDARRAYHF          0x0B                                            // непрерывное чтение массива (высокая частота) 
#define AT45_RDBF1LF            0xD1                                            // чтение буфера 1 (низкая частота) 
#define AT45_RDBF2LF            0xD3                                            // чтение буфера 2 (низкая частота) 
#define AT45_RDBF1              0xD4                                            // чтение буфера 1
#define AT45_RDBF2              0xD6                                            // чтение буфера 2
                                                                            
// команды записи и программирования
#define AT45_WRBF1              0x84                                            // запись буфера 1
#define AT45_WRBF2              0x87                                            // запись буфера 2
#define AT45_BF1TOMNE           0x83                                            // программирование со встроенной функцией стирания из Буфера 1 на основную страницу памяти
#define AT45_BF2TOMNE           0x86                                            // программирование со встроенной функцией стирания из Буфера 2 на основную страницу памяти
#define AT45_BF1TOMN            0x88                                            // программирование без встроенного стирания из Буфера 1 на основную страницу памяти
#define AT45_BF2TOMN            0x89                                            // программирование без встроенного стирания из Буфера 2 на основную страницу памяти
#define AT45_PGERASE            0x81                                            // стирание страницы
#define AT45_BLKERASE           0x50                                            // стирание блока
#define AT45_SECTERASE          0x7C                                            // стирание сектора
#define AT45_CHIPERASE1         0xC7                                            // стирание чипа - байт 1 
#define AT45_CHIPERASE2         0x94                                            // стирание чипа - байт 2 
#define AT45_CHIPERASE3         0x80                                            // стирание чипа - байт 3 
#define AT45_CHIPERASE4         0x9A                                            // стирание чипа - байт 4 
#define AT45_MNTHRUBF1          0x82                                            // программирование основной страницы памяти через Буфер 1 со встроенным стиранием
#define AT45_MNTHRUBF2          0x85                                            // программирование основной страницы памяти через Буфер 2 со встроенным стиранием
                                                                            
// команды защиты и безопасности
#define AT45_ENABPROT1          0x3D                                            // включение  защиты сектора - байт 1
#define AT45_ENABPROT2          0x2A                                            // включение  защиты сектора - байт 2 
#define AT45_ENABPROT3          0x7F                                            // включение  защиты сектора - байт 3 
#define AT45_ENABPROT4          0xA9                                            // включение  защиты сектора - байт 4 
#define AT45_DISABPROT1         0x3D                                            // выключение защиты сектора - байт 1 
#define AT45_DISABPROT2         0x2A                                            // выключение защиты сектора - байт 2 
#define AT45_DISABPROT3         0x7F                                            // выключение защиты сектора - байт 3 
#define AT45_DISABPROT4         0x9A                                            // выключение защиты сектора - байт 4 
#define AT45_ERASEPROT1         0x3D                                            // стирание регистра защиты сектора - байт 1 
#define AT45_ERASEPROT2         0x2A                                            // стирание регистра защиты сектора - байт 2 
#define AT45_ERASEPROT3         0x7F                                            // стирание регистра защиты сектора - байт 3 
#define AT45_ERASEPROT4         0xCF                                            // стирание регистра защиты сектора - байт 4 
#define AT45_PROGPROT1          0x3D                                            // программирование регистра защиты сектора - байт 1 
#define AT45_PROGPROT2          0x2A                                            // программирование регистра защиты сектора - байт 2 
#define AT45_PROGPROT3          0x7F                                            // программирование регистра защиты сектора - байт 3 
#define AT45_PROGPROT4          0xFC                                            // программирование регистра защиты сектора - байт 4 
#define AT45_RDPROT             0x32                                            // чтение регистра защиты сектора
#define AT45_LOCKDOWN1          0x3D                                            // блокировка сектора - байт 1 
#define AT45_LOCKDOWN2          0x2A                                            // блокировка сектора - байт 2 
#define AT45_LOCKDOWN3          0x7F                                            // блокировка сектора - байт 3 
#define AT45_LOCKDOWN4          0x30                                            // блокировка сектора - байт 4 
#define AT45_RDLOCKDOWN         0x35                                            // чтение регистра блокировки сектора
#define AT45_PROGSEC1           0x9B                                            // программирование регистра безопасности - байт 1 
#define AT45_PROGSEC2           0x00                                            // программирование регистра безопасности - байт 2 
#define AT45_PROGSEC3           0x00                                            // программирование регистра безопасности - байт 3 
#define AT45_PROGSEC4           0x00                                            // программирование регистра безопасности - байт 4 
#define AT45_RDSEC              0x77                                            // чтение регистра безопасности
                                                                            
// дополнительные команды
#define AT45_MNTOBF1XFR         0x53                                            // передача страницы основной памяти в буфер 1
#define AT45_MNTOBF2XFR         0x55                                            // передача страницы основной памяти в буфер 2
#define AT45_MNBF1CMP           0x60                                            // сравнение страницы основной памяти с буфером 1
#define AT45_MNBF2CMP           0x61                                            // сравнение страницы основной памяти с буфером 2
#define AT45_AUTOWRBF1          0x58                                            // автоматическая перезапись страницы через буфер 1
#define AT45_AUTOWRBF2          0x59                                            // автоматическая перезапись страницы через буфер 2
#define AT45_RDSR               0xD7                                            // чтение регистра состояния

// идентификаторы, маски
#define AT45_ADESTO             0x1F                                            // идентификатор производителя: Atmel
#define AT45_DEVID1_CAPMSK      0x1F                                            // битовая маска. биты 0-4 описывают объём памяти микросхемы.
#define AT45_DEVID1_FAMMSK      0xE0                                            // битовая маска. биты 5-7 описывают семейство микросхемы.
#define AT45_DEVID1_AT45DB      0x20                                            // 001x xxxx = AT45Dxxxx family 
#define AT45_DEVID1_AT26DF      0x40                                            // 010x xxxx = AT26Dxxxx family (не поддерживается) 
#define AT45_DEVID2_VERMSK      0x1F                                            // биты 0-4: MLC mask 
#define AT45_DEVID2_MLCMSK      0xE0                                            // биты 5-7: MLC mask 

// определения битов регистра состояния
#define AT45_SR_RDY             (1 << 7)                                        // бит 7: RDY/ Not BUSY 
#define AT45_SR_COMP            (1 << 6)                                        // бит 6: COMP 
#define AT45_SR_PROTECT         (1 << 1)                                        // бит 1: PROTECT 
#define AT45_SR_PGSIZE          (1 << 0)                                        // бит 0: PAGE_SIZE 

// -------------- определения для м/сх FLASH-памяти серии W25QXXX --------------------
// определения команд для м/сх серии w25qXXX
#define W25_WINBOND             0xEF                                            // идентификатор производителя: Winbond

#define W25_WRDI                0x04                                            // запрет записи в м/сх памяти
#define W25_WREN                0x06                                            // разрешение записи в м/сх памяти
#define W25_CE                  0xC7                                            // очистка чипа (альтернативная команда: 0x60)
#define W25_SE                  0x20                                            // очистка указанного сектора.                  A[23:16], A[15:8], A[7:0]
#define W25_BE                  0xD8                                            // очистка указанного 64Кб блока.               A[23:16], A[15:8], A[7:0]
#define W25_FAST_READ           0x0B                                            // быстрое чтение данных.                       A[23:16], A[15:8], A[7:0], DUMMY, D7-D0
#define W25_PP                  0x02                                            // программирование заданной страницы памяти.   A[23:16], A[15:8], A[7:0], D7-D0, D7-D0
#define W25_RDSR1               0x05                                            // чтение регистра статуса 1.                   S[7:0]
#define W25_WRSR1               0x01                                            // запись регистра статуса 1.                   S[7:0]
#define W25_RDSR2               0x35                                            // чтение регистра статуса 2.                   S[15:8]
#define W25_WRSR2               0x31                                            // запись регистра статуса 2.                   S[15:8]
#define W25_RDSR3               0x15                                            // чтение регистра статуса 3.                   S[23:16]
#define W25_WRSR3               0x11                                            // запись регистра статуса 3.                   S[23:16]

// определения битов регистра состояния
#define W25_SR1S0               (1 << 0)                                        // бит 0 регистра статуса 1: BUSY

// -------------- определения для м/сх FLASH-памяти серии MX25LXXXX --------------------
// определения команд для м/сх серии mx25lXXXX
#define MX25_MACRONIX           0xC2                                            // идентификатор производителя: Macronix


// -----------------------------------------------------------------------------

bool    Flash_Init      (void);
void    Flash_Resume    (void);
void    Flash_PowerDown (void);
void    Flash_EraseChip (void);
void    Flash_EraseArea (uint32_t page);
void    Flash_WritePage (uint32_t page, uint32_t offset, uint32_t size, uint8_t *buf);
void    Flash_ReadPage  (uint32_t page, uint32_t offset, uint32_t size, uint8_t *buf);


// -----------------------------------------------------------------------------

int block_device_read   (const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
int block_device_prog   (const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);
int block_device_erase  (const struct lfs_config *c, lfs_block_t block);
int block_device_sync   (const struct lfs_config *c);

#endif
