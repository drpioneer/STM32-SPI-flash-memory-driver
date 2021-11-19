/*
 *  Драйвер для работы с м/сх FLASH-памяти at45dbXXX от Adesto!!! (есть небольшие отличия в работе с памятью от Atmel)
 *  Источники кода:     https://github.com/nimaltd/45dbxxx
 *                      https://github.com/iDiy/AT45DB161D-Drv
 *  Описание работы:    http: //we.easyelectronics.ru/Frankie/spi-programmnyy-pamyat-atmel-dataflash-at45db081d.html
 *                      https://eax.me/stm32-spi-flash/
 *  Настройки SPI1 в CubeMX для работы с микросхемой FLASH-памяти:
 *                      Frame format: Motorola      Data size: 8 bit            First bit: MSB;
 *                      CPOL: LOW                   CPHA:      1 Edge
 *                      CRC:  Disabled              NSS:       Software
 *  Особенности работы с м/сх:
 *  1.  По умолчанию вся память организована по 264/528/1056 байт на страницу. 
 *      Размер страницы можно однократно изменить на 256/512/1024 байт без возможности вернуться обратно.
 *  2.  Микросхема имеет специальные 4 байта для однократной записи идентификатора (например, серийного номера изделия).
 *  3.  Обмен данными производится по 4-х проводному интерфейсу SPI. Микросхема может работать в режимах SPI 0 и 3, 
 *      а также двух собственных режимах Atmel. В коде использован обычный SPI mode 0.
 *  4.  Организация памяти страничная -> доступ к отдельным ячейкам памяти не существует.
 *      Все операции чтения/записи происходят по принципу «чтение-модификация-запись».
 *  5.  После подачи питания и перед обращением к памяти необходимо сделать гарантированную задержку в 20 миллисекунд.
 *  Выявленное отличие м/сх Adesto от м/сх Atmel:   регистр состояния 16-битный, а не 8.
 */ 

#include "spi.h"
#include "45dbxx.h"                                                             // Библиотека драйвер FLASH-памяти

#define _45DBXX_SPI             hspi1                                           // Микросхема FLASH-памяти подключена к SPI1
#define _45DBXX_CS_GPIO         SPI1_NSS_GPIO_Port                              // Настройка программного NSS SPI1
#define _45DBXX_CS_PIN          SPI1_NSS_Pin

#define _45DBXX_USE_FREERTOS    1                                               // =1 -> работа в многозадачной среде
#if (_45DBXX_USE_FREERTOS == 1)
    #include "cmsis_os.h"
    #define _45DBXX_DELAY(x)    osDelay(x)
#else
    #define _45DBXX_DELAY(x)    HAL_Delay(x)
#endif

// -------------- Определение команд для FLASH-памяти --------------------
// Команды чтения
#define AT45DB_RDMN             0xD2                                            // Чтение основной страницы памяти
#define AT45DB_RDARRY           0xE8                                            // Непрерывное чтение массива (унаследованная команда) 
#define AT45DB_RDARRAYLF        0x03                                            // Непрерывное чтение массива (низкая частота) 
#define AT45DB_RDARRAYHF        0x0B                                            // Непрерывное чтение массива (высокая частота) 
#define AT45DB_RDBF1LF          0xD1                                            // Чтение Буфера 1 (низкая частота) 
#define AT45DB_RDBF2LF          0xD3                                            // Чтение Буфера 2 (низкая частота) 
#define AT45DB_RDBF1            0xD4                                            // Чтение Буфера 1
#define AT45DB_RDBF2            0xD6                                            // Чтение Буфера 2
                                                                                
// Команды записи и программирования
#define AT45DB_WRBF1            0x84                                            // Запись Буфера 1
#define AT45DB_WRBF2            0x87                                            // Запись Буфера 2
#define AT45DB_BF1TOMNE         0x83                                            // Программирование со встроенной функцией стирания из Буфера 1 на основную страницу памяти
#define AT45DB_BF2TOMNE         0x86                                            // Программирование со встроенной функцией стирания из Буфера 2 на основную страницу памяти
#define AT45DB_BF1TOMN          0x88                                            // Программирование без встроенного стирания из Буфера 1 на основную страницу памяти
#define AT45DB_BF2TOMN          0x89                                            // Программирование без встроенного стирания из Буфера 2 на основную страницу памяти
#define AT45DB_PGERASE          0x81                                            // Стирание страницы
#define AT45DB_BLKERASE         0x50                                            // Стирание блока
#define AT45DB_SECTERASE        0x7C                                            // Стирание сектора
#define AT45DB_CHIPERASE1       0xC7                                            // Стирание чипа - байт 1 
#define AT45DB_CHIPERASE2       0x94                                            // Стирание чипа - байт 2 
#define AT45DB_CHIPERASE3       0x80                                            // Стирание чипа - байт 3 
#define AT45DB_CHIPERASE4       0x9A                                            // Стирание чипа - байт 4 
#define AT45DB_MNTHRUBF1        0x82                                            // Программирование основной страницы памяти через Буфер 1 со встроенным стиранием
#define AT45DB_MNTHRUBF2        0x85                                            // Программирование основной страницы памяти через Буфер 2 со встроенным стиранием
                                                                                
// Команды защиты и безопасности
#define AT45DB_ENABPROT1        0x3D                                            // Включение  защиты сектора - байт 1
#define AT45DB_ENABPROT2        0x2A                                            // Включение  защиты сектора - байт 2 
#define AT45DB_ENABPROT3        0x7F                                            // Включение  защиты сектора - байт 3 
#define AT45DB_ENABPROT4        0xA9                                            // Включение  защиты сектора - байт 4 
#define AT45DB_DISABPROT1       0x3D                                            // Выключение защиты сектора - байт 1 
#define AT45DB_DISABPROT2       0x2A                                            // Выключение защиты сектора - байт 2 
#define AT45DB_DISABPROT3       0x7F                                            // Выключение защиты сектора - байт 3 
#define AT45DB_DISABPROT4       0x9A                                            // Выключение защиты сектора - байт 4 
#define AT45DB_ERASEPROT1       0x3D                                            // Стирание регистра защиты сектора - байт 1 
#define AT45DB_ERASEPROT2       0x2A                                            // Стирание регистра защиты сектора - байт 2 
#define AT45DB_ERASEPROT3       0x7F                                            // Стирание регистра защиты сектора - байт 3 
#define AT45DB_ERASEPROT4       0xCF                                            // Стирание регистра защиты сектора - байт 4 
#define AT45DB_PROGPROT1        0x3D                                            // Программирование регистра защиты сектора - байт 1 
#define AT45DB_PROGPROT2        0x2A                                            // Программирование регистра защиты сектора - байт 2 
#define AT45DB_PROGPROT3        0x7F                                            // Программирование регистра защиты сектора - байт 3 
#define AT45DB_PROGPROT4        0xFC                                            // Программирование регистра защиты сектора - байт 4 
#define AT45DB_RDPROT           0x32                                            // Чтение регистра защиты сектора
#define AT45DB_LOCKDOWN1        0x3D                                            // Блокировка сектора - байт 1 
#define AT45DB_LOCKDOWN2        0x2A                                            // Блокировка сектора - байт 2 
#define AT45DB_LOCKDOWN3        0x7F                                            // Блокировка сектора - байт 3 
#define AT45DB_LOCKDOWN4        0x30                                            // Блокировка сектора - байт 4 
#define AT45DB_RDLOCKDOWN       0x35                                            // Чтение регистра блокировки сектора
#define AT45DB_PROGSEC1         0x9B                                            // Программирование регистра безопасности - байт 1 
#define AT45DB_PROGSEC2         0x00                                            // Программирование регистра безопасности - байт 2 
#define AT45DB_PROGSEC3         0x00                                            // Программирование регистра безопасности - байт 3 
#define AT45DB_PROGSEC4         0x00                                            // Программирование регистра безопасности - байт 4 
#define AT45DB_RDSEC            0x77                                            // Чтение регистра безопасности
                                                                                
// Дополнительные команды
#define AT45DB_MNTOBF1XFR       0x53                                            // Передача страницы основной памяти в Буфер 1
#define AT45DB_MNTOBF2XFR       0x55                                            // Передача страницы основной памяти в Буфер 2
#define AT45DB_MNBF1CMP         0x60                                            // Сравнение страницы основной памяти с Буфером 1
#define AT45DB_MNBF2CMP         0x61                                            // Сравнение страницы основной памяти с Буфером 2
#define AT45DB_AUTOWRBF1        0x58                                            // Автоматическая перезапись страницы через Буфер 1
#define AT45DB_AUTOWRBF2        0x59                                            // Автоматическая перезапись страницы через Буфер 2
#define AT45DB_PWRDOWN          0xB9                                            // Глубокое отключение питания
#define AT45DB_RESUME           0xAB                                            // Возобновление после глубокого выключения питания
#define AT45DB_RDSR             0xD7                                            // Чтение регистра состояния
#define AT45DB_RDDEVID          0x9F                                            // Чтение идентификатора производителя и устройства

// Идентификаторы, маски
#define AT45DB_MANUFACTURER     0x1F                                            // Идентификатор производителя: Atmel
#define AT45DB_DEVID1_CAPMSK    0x1F                                            // Битовая маска. Биты 0-4 описывают объём памяти микросхемы.
#define AT45DB_DEVID1_1MBIT     0x02                                            // xxx0 0010 = 1  Мбит AT45DB011 
#define AT45DB_DEVID1_2MBIT     0x03                                            // xxx0 0012 = 2  Мбит AT45DB021 
#define AT45DB_DEVID1_4MBIT     0x04                                            // xxx0 0100 = 4  Мбит AT45DB041 
#define AT45DB_DEVID1_8MBIT     0x05                                            // xxx0 0101 = 8  Мбит AT45DB081 
#define AT45DB_DEVID1_16MBIT    0x06                                            // xxx0 0110 = 16 Мбит AT45DB161 
#define AT45DB_DEVID1_32MBIT    0x07                                            // xxx0 0111 = 32 Мбит AT45DB321 
#define AT45DB_DEVID1_64MBIT    0x08                                            // xxx0 1000 = 32 Мбит AT45DB641 
#define AT45DB_DEVID1_FAMMSK    0xE0                                            // Битовая маска. Биты 5-7 описывают семейство микросхемы.
#define AT45DB_DEVID1_DFLASH    0x20                                            // 001x xxxx = AT45Dxxxx family 
#define AT45DB_DEVID1_AT26DF    0x40                                            // 010x xxxx = AT26Dxxxx family (не поддерживается) 
#define AT45DB_DEVID2_VERMSK    0x1F                                            // Биты 0-4: MLC mask 
#define AT45DB_DEVID2_MLCMSK    0xE0                                            // Биты 5-7: MLC mask 

// Определения битов регистра состояния
#define AT45DB_SR_RDY           (1 << 7)                                        // Бит 7: RDY/ Not BUSY 
#define AT45DB_SR_COMP          (1 << 6)                                        // Бит 6: COMP 
#define AT45DB_SR_PROTECT       (1 << 1)                                        // Бит 1: PROTECT 
#define AT45DB_SR_PGSIZE        (1 << 0)                                        // Бит 0: PAGE_SIZE 

AT45_t                  AT45;

// ------------------------------- Базовые функции для работы с м/сх at45dbXXX: -----------------------------------------------------

uint8_t AT45_Spi        (uint8_t data)                                          // Функция одновременного чтения/отправки посылки из 8 бит из/в микросхемы(у) FLASH-памяти
{
    uint8_t             ret = 0;
    HAL_SPI_TransmitReceive(&_45DBXX_SPI, &data, &ret, 1, 100);                 // Отправка/чтение заданной команды/данных
    return ret;
}

uint16_t AT45_ReadStatus(void)                                                  // Функция определения статуса микросхемы FLASH-памяти.
{                                                                               
    uint8_t             status1 = 0, status2 = 0;
    HAL_GPIO_WritePin   (_45DBXX_CS_GPIO, _45DBXX_CS_PIN, GPIO_PIN_RESET);      // ----- Начало работы с м/сх FLASH-памяти -----
    AT45_Spi            (AT45DB_RDSR);                                          // Отправка команды: 0xD7 - "Считывание регистра состояния"
    status1             = AT45_Spi(0x00);                                       // Получение 1-го байта ответа от м/сх (как у Atmel)
    status2             = AT45_Spi(0x00);                                       // Получение 2-го байта ответа от м/сх (только у Adesto)
    HAL_GPIO_WritePin   (_45DBXX_CS_GPIO, _45DBXX_CS_PIN, GPIO_PIN_SET);        // ----- Завершение работы с м/сх FLASH-памяти -----
    return (( (status2 & 0xFF) << 8) | (status1 & 0xFF));                       // Возврат 16 бит статуса м/сх
}

bool AT45_IsBusy        (void)                                                  // Функция определения готовности микросхемы FLASH-памяти
{                                                                               // Необходимость функции продиктована продолжительным процессом записи. Для определения его окончания нужна эта функция.
    return ((AT45_ReadStatus() & AT45DB_SR_RDY) == 0);                          // Возврат TRUE, когда м/сх готова для принятия очередной команды
}

void AT45_Resume        (void)                                                  // Функция возобновления работы микросхемы FLASH-памяти после глубокого выключения питания
{
    HAL_GPIO_WritePin   (_45DBXX_CS_GPIO, _45DBXX_CS_PIN, GPIO_PIN_RESET);    
    AT45_Spi            (AT45DB_RESUME);                                        // Отправка команды: 0xAB - "Возобновление после глубокого выключения питания"
    HAL_GPIO_WritePin   (_45DBXX_CS_GPIO, _45DBXX_CS_PIN, GPIO_PIN_SET);
    while (AT45_IsBusy());                                                      // Ожидание готовности микросхемы FLASH-памяти
}

void AT45_PowerDown (void)                                                      // Функция глубокого выключения питания микросхемы FLASH-памяти
{
    HAL_GPIO_WritePin   (_45DBXX_CS_GPIO, _45DBXX_CS_PIN, GPIO_PIN_RESET);    
    AT45_Spi            (AT45DB_PWRDOWN);                                       // Отправка команды: 0xB9 - "Глубокое отключение питания"
    HAL_GPIO_WritePin   (_45DBXX_CS_GPIO, _45DBXX_CS_PIN, GPIO_PIN_SET);    
}

bool AT45_Init          (void)                                                  // Функция инициализации микросхемы FLASH-памяти
{
    HAL_GPIO_WritePin   (_45DBXX_CS_GPIO, _45DBXX_CS_PIN, GPIO_PIN_SET);
    while (HAL_GetTick() < 20)
    {
        _45DBXX_DELAY   (10);                                                   // Задержка перед инициализацией м/сх
    }
    uint8_t             mnfID = 0, prdID = 0, edi = 0;
    HAL_GPIO_WritePin   (_45DBXX_CS_GPIO, _45DBXX_CS_PIN, GPIO_PIN_RESET);      // ----- Начало работы с м/сх FLASH-памяти -----
    AT45_Spi            (AT45DB_RDDEVID);                                       // Отправка команды: 0x9F - "Считывание идентификатора производителя и устройства"
    mnfID               = AT45_Spi(0x00);                                       // Получение 1-го байта ответа от м/сх: идентификатор производителя
    prdID               = AT45_Spi(0x00);                                       // Получение 2-го байта ответа от м/сх: идентификатор устройства 1-й байт
                          AT45_Spi(0x00);                                       // Получение 3-го байта ответа от м/сх: идентификатор устройства 2-й байт - НЕ ИНТЕРЕСУЕТ
                          AT45_Spi(0x00);                                       // Получение 4-го байта ответа от м/сх: длина поля расширенных данных EDI - НЕ ИНТЕРЕСУЕТ
    edi                 = AT45_Spi(0x00);                                       // Получение 5-го байта ответа от м/сх: расширенные данные EDI
    HAL_GPIO_WritePin   (_45DBXX_CS_GPIO, _45DBXX_CS_PIN, GPIO_PIN_SET);        // ----- Завершение работы с м/сх FLASH-памяти -----

    if (mnfID == AT45DB_MANUFACTURER)                                           // Когда идентификатор производителя = 0x1F (сейчас это Adesto, ранее был Atmel) ->
    {
        if ((prdID & AT45DB_DEVID1_FAMMSK) == AT45DB_DEVID1_DFLASH)             // Когда семейство м/сх соответствует AT45DBxxx ->
        {
            switch    (prdID & AT45DB_DEVID1_CAPMSK)                            // Выяснение объёма м/сх FLASH-памяти путём наложения битовой маски на 5 младших битов 1-го байта идентификатора устройства                                              
            {                                                                                                                                                                                                  
                case AT45DB_DEVID1_1MBIT:                                       // xxx0 0010 => 1Mbit AT45DB011 (этот кейс отсутствовал и был дописан по аналогии со следующими. Возможны ошибки.)
                    AT45.FlashSize_MBit = 1;
                    AT45.Pages          = 512;
                    if (edi & 0x01)
                    {
                        AT45.Shift      = 8;
                        AT45.PageSize   = 256;
                    }
                    else
                    {
                        AT45.Shift      = 9;
                        AT45.PageSize   = 264;
                    }
                break;

                case AT45DB_DEVID1_2MBIT:                                       // xxx0 0012 => 2Mbit AT45DB021
                    AT45.FlashSize_MBit = 2;
                    AT45.Pages          = 1024;
                    if (edi & 0x01)
                    {
                        AT45.Shift      = 8;
                        AT45.PageSize   = 256;
                    }
                    else
                    {
                        AT45.Shift      = 9;
                        AT45.PageSize   = 264;
                    }
                break;
               
                case AT45DB_DEVID1_4MBIT:                                       // xxx0 0100 => 4Mbit AT45DB041
                    AT45.FlashSize_MBit = 4;
                    AT45.Pages          = 2048;
                    if (edi & 0x01)
                    {
                        AT45.Shift      = 8;
                        AT45.PageSize   = 256;
                    }
                    else
                    {
                        AT45.Shift      = 9;
                        AT45.PageSize   = 264;
                    }
                break;

                case AT45DB_DEVID1_8MBIT:                                       // xxx0 0101 => 8Mbit AT45DB081
                    AT45.FlashSize_MBit = 8;
                    AT45.Pages          = 4096;
                    if (edi & 0x01)
                    {
                        AT45.Shift      = 8;
                        AT45.PageSize   = 256;
                    }
                    else
                    {
                        AT45.Shift      = 9;
                        AT45.PageSize   = 264;
                    }
                break;

                case AT45DB_DEVID1_16MBIT:                                      // xxx0 0110 => 16Mbit AT45DB161
                    AT45.FlashSize_MBit = 16;
                    AT45.Pages          = 4096;
                    if (edi & 0x01)
                    {
                        AT45.Shift      = 9;
                        AT45.PageSize   = 512;
                    }
                    else
                    {
                        AT45.Shift      = 10;
                        AT45.PageSize   = 528;
                    }
                break;

               case AT45DB_DEVID1_32MBIT:                                       // xxx0 0111 => 32Mbit AT45DB321
                    AT45.FlashSize_MBit = 32;
                    AT45.Pages          = 8192;
                    if (edi & 0x01)
                    {
                        AT45.Shift      = 9;
                        AT45.PageSize   = 512;
                    }
                    else
                    {
                        AT45.Shift      = 10;
                        AT45.PageSize   = 528;
                    }
                break;

                case AT45DB_DEVID1_64MBIT:                                      // xxx0 1000 => 32Mbit AT45DB641
                    AT45.FlashSize_MBit = 64;
                    AT45.Pages          = 8192;
                    if (edi & 0x01)
                    {
                        AT45.Shift      = 10;
                        AT45.PageSize   = 1024;
                    }
                    else
                    {
                        AT45.Shift      = 11;
                        AT45.PageSize   = 1056;
                    }
                break;            
            }
        }
        return true;                                                            // Возврат успешной инициализации м/сх
    }
    else
    {
        return false;                                                           // Возврат неудачной инициализации м/сх
    }
}

void AT45_EraseChip     (void)                                                  // Функция стирания всех данных из микросхемы FLASH-памяти
{
    AT45_Resume         ();                                                     // Пробуждение микросхемы FLASH-памяти
    HAL_GPIO_WritePin   (_45DBXX_CS_GPIO, _45DBXX_CS_PIN, GPIO_PIN_RESET);      // ----- Начало работы с м/сх FLASH-памяти -----
    AT45_Spi            (AT45DB_CHIPERASE1);                                    // Отправка команды: "Стирание чипа - байт 1"
    AT45_Spi            (AT45DB_CHIPERASE2);                                    // Отправка команды: "Стирание чипа - байт 2"
    AT45_Spi            (AT45DB_CHIPERASE3);                                    // Отправка команды: "Стирание чипа - байт 3"
    AT45_Spi            (AT45DB_CHIPERASE4);                                    // Отправка команды: "Стирание чипа - байт 4"
    HAL_GPIO_WritePin   (_45DBXX_CS_GPIO, _45DBXX_CS_PIN, GPIO_PIN_SET);        // ----- Завершение работы с м/сх FLASH-памяти -----
    while (AT45_IsBusy());                                                      // Ожидание готовности микросхемы FLASH-памяти
}

void AT45_ErasePage     (uint16_t page)                                         // Функция стирания указанной страницы на микросхеме FLASH-памяти
{
    uint32_t addr       = ((uint32_t)page) << AT45.Shift;                       // Вычисление абсолютного адреса
    AT45_Resume         ();                                                     // Пробуждение микросхемы FLASH-памяти
    HAL_GPIO_WritePin   (_45DBXX_CS_GPIO, _45DBXX_CS_PIN, GPIO_PIN_RESET);      // ----- Начало работы с м/сх FLASH-памяти -----
    AT45_Spi            (AT45DB_PGERASE);                                       // Отправка команды: 0x81 - "Стирание страницы"
    AT45_Spi            ((uint8_t)(addr >> 16));                                // 1й операнд для at45db081: отправка 3 фиктивных битов + PA11-PA7
    AT45_Spi            ((uint8_t)(addr >>  8));                                // 2й операнд для at45db081: отправка PA6-PA0 + BA8
    AT45_Spi            ((uint8_t)(addr      ));                                // 3й операнд для at45db081: отправка BA7-BA0
    HAL_GPIO_WritePin   (_45DBXX_CS_GPIO, _45DBXX_CS_PIN, GPIO_PIN_SET);        // ----- Завершение работы с м/сх FLASH-памяти -----
    while (AT45_IsBusy  ());                                                    // Ожидание готовности микросхемы FLASH-памяти
}

void AT45_WritePage     (uint16_t page, uint16_t offset, uint16_t size, uint8_t *buf) // Функция записи данных на микросхему FLASH-памяти в указанную страницу с заданным смещением
{
    if (size > AT45.PageSize)   size = AT45.PageSize;                           // Проверка размера блока
    uint32_t addr       = (((uint32_t)page) << AT45.Shift) + offset;            // Вычисление абсолютного адреса
    AT45_Resume         ();                                                     // Пробуждение микросхемы FLASH-памяти
    HAL_GPIO_WritePin   (_45DBXX_CS_GPIO, _45DBXX_CS_PIN, GPIO_PIN_RESET);      // ----- Начало работы с м/сх FLASH-памяти -----
    AT45_Spi            (AT45DB_MNTHRUBF1);                                     // Отправка команды: 0x82 - "Программирование основной страницы памяти через Буфер 1 со встроенным стиранием"
    AT45_Spi            ((uint8_t)(addr >> 16));                                // 1й операнд для at45db081: отправка 3 фиктивных битов + PA11-PA7
    AT45_Spi            ((uint8_t)(addr >>  8));                                // 2й операнд для at45db081: отправка PA6-PA0 + BA8
    AT45_Spi            ((uint8_t)(addr      ));                                // 3й операнд для at45db081: отправка BA7-BA0
    HAL_SPI_Transmit    (&_45DBXX_SPI, buf, size, 100);                         // Запись массива данных в м/сх
    HAL_GPIO_WritePin   (_45DBXX_CS_GPIO, _45DBXX_CS_PIN, GPIO_PIN_SET);        // ----- Завершение работы с м/сх FLASH-памяти -----
}

void AT45_ReadPage      (uint16_t page, uint16_t offset, uint16_t size, uint8_t *buf) // Функция чтения данных из микросхемы FLASH-памяти с указанной страницы с заданным смещением
{
    if (size > AT45.PageSize)   size = AT45.PageSize;                           // Проверка размера блока
    uint32_t addr       = (((uint32_t)page) << AT45.Shift) + offset;            // Вычисление абсолютного адреса
    AT45_Resume         ();                                                     // Пробуждение микросхемы FLASH-памяти
    HAL_GPIO_WritePin   (_45DBXX_CS_GPIO, _45DBXX_CS_PIN, GPIO_PIN_RESET);      // ----- Начало работы с м/сх FLASH-памяти -----
    AT45_Spi            (AT45DB_RDARRAYHF);                                     // Отправка команды:  0x0B - "Непрерывное чтение массива (High Frequency)"
    AT45_Spi            ((uint8_t)(addr >> 16));                                // 1й операнд для at45db081: отправка 3 фиктивных битов + PA11-PA7
    AT45_Spi            ((uint8_t)(addr >>  8));                                // 2й операнд для at45db081: отправка PA6-PA0 + BA8
    AT45_Spi            ((uint8_t)(addr      ));                                // 3й операнд для at45db081: отправка BA7-BA0
    AT45_Spi            (0);                                                    // 4й операнд для at45db081: отправка 8 фиктивных битов
    HAL_SPI_Receive     (&_45DBXX_SPI, buf, size, 100);                         // Чтение запрошенного массива данных из м/сх
    HAL_GPIO_WritePin   (_45DBXX_CS_GPIO, _45DBXX_CS_PIN, GPIO_PIN_SET);        // ----- Завершение работы с м/сх FLASH-памяти -----
}

// ------------------------------- Функции-прокладки для связки LittleFS и at45dbXXX: -----------------------------------------------------

int block_device_read(const struct lfs_config *c, lfs_block_t block,            // Функция-прокладка для чтения страницы с носителя.
	lfs_off_t off, void *buffer, lfs_size_t size)
{
    AT45_ReadPage((uint16_t)block, (uint16_t)off, (uint16_t)size, (uint8_t*)buffer); // Чтение данных на указанной странице с заданным смещением и определенной длины 
	return 0;
}

int block_device_prog(const struct lfs_config *c, lfs_block_t block,            // Функция-прокладка для записи страницы на носитель. Страница должна быть заранее очищена.
	lfs_off_t off, const void *buffer, lfs_size_t size)
{
    AT45_WritePage((uint16_t)block, (uint16_t)off, (uint16_t)size, (uint8_t*)buffer); // Запись данных на указанную страницу с заданным смещением и определенной длины 
	return 0;
}

int block_device_erase(const struct lfs_config *c, lfs_block_t block)           // Функция-прокладка для удаления области, например, перед записью. Состояние стертого блока не определено.
{
    AT45_ErasePage((uint16_t)block);                                            // Очистка указанной страницы
	return 0;
}

int block_device_sync(const struct lfs_config *c)                               // Функция-прокладка для синхронизации состояния носителя. Необходима только для носителей с кэшем (не наш случай).
{
	return 0;
}
// ------------------------------------------------------------------------------------------------------------------------------------------
