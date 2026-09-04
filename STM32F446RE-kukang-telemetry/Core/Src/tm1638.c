#include "tm1638.h"
#include "main.h"
#include <string.h>

extern SPI_HandleTypeDef hspi2;

// Buffer untuk menampung paket data DMA
// [0] = Command Address Set (0xC0)
// [1..16] = Data display (Seg1..Seg16)
static uint8_t dma_buffer[17];

// Status apakah sedang transfer (untuk mencegah overlapping call)
static volatile uint8_t is_transferring = 0;

// Definisi font
const uint8_t TM1638_FONT[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D,
                               0x7D, 0x07, 0x7F, 0x6F, 0x77, 0x7C,
                               0x39, 0x5E, 0x79, 0x71, 0x00, 0x40};

// Helper Macros
#define STB_LOW()                                                              \
  HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_RESET)
#define STB_HIGH()                                                             \
  HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_SET)

// --- Fungsi Init (Blocking tidak masalah karena cuma sekali) ---
void TM1638_Init(void) {
  STB_HIGH();
  // Kirim command display control (Blocking)
  uint8_t cmd = 0x8F; // Display ON, Brightness Max
  STB_LOW();
  HAL_SPI_Transmit(&hspi2, &cmd, 1, 10);
  STB_HIGH();

  // Set Data Command ke Auto Increment Address (Blocking)
  // Ini penting agar DMA bisa kirim stream data berturut-turut
  cmd = 0x40;
  STB_LOW();
  HAL_SPI_Transmit(&hspi2, &cmd, 1, 10);
  STB_HIGH();

  TM1638_Clear();
}

// --- Fungsi Kirim DMA (Updated with Float/Dot support) ---
void TM1638_SendDMA(const char *str, uint8_t leds) {
  if (is_transferring)
    return; // Jangan kirim jika sebelumnya belum selesai

  // 1. Siapkan Buffer
  dma_buffer[0] = 0xC0; // Command: Set Address to 00H

  // 2. Isi Buffer Data Display
  int str_idx = 0; // Index untuk menelusuri string input
  int str_len = strlen(str);

  for (int i = 0; i < 8; i++) // Loop untuk 8 Grid/Digit di TM1638
  {
    uint8_t seg_data = 0;

    // Pastikan kita tidak membaca di luar panjang string
    if (str_idx < str_len) {
      char c = str[str_idx];

      // A. Mapping Karakter ke 7-Segment
      if (c >= '0' && c <= '9')
        seg_data = TM1638_FONT[c - '0'];
      else if (c >= 'A' && c <= 'F')
        seg_data = TM1638_FONT[c - 'A' + 10];
      else if (c >= 'a' && c <= 'f') // Tambahan: support huruf kecil a-f
        seg_data = TM1638_FONT[c - 'a' + 10];
      else if (c == 'N' || c == 'n')
        seg_data = 0x54;
      else if (c == 'R' || c == 'r')
        seg_data = 0x50;
      else if (c == 'S' || c == 's')
        seg_data = 0x6D; // Looks like 5
      else if (c == 'I' || c == 'i')
        seg_data = 0x06; // Looks like 1
      else if (c == 'X' || c == 'x')
        seg_data = 0x76; // Looks like H (proxy for X)
      else if (c == 'O')
        seg_data = 0x3F; // Looks like 0
      else if (c == 'o')
        seg_data = 0x5C; // Lowercase o
      else if (c == 'P' || c == 'p')
        seg_data = 0x73;
      else if (c == 'L' || c == 'l')
        seg_data = 0x38;
      else if (c == 'T' || c == 't')
        seg_data = 0x78;
      else if (c == '-')
        seg_data = 0x40;
      else if (c == ' ')
        seg_data = 0x00;
      // Jika karakter adalah titik di awal (misal ".5"), anggap '0' lalu titik
      else if (c == '.' || c == ',')
        seg_data = TM1638_FONT[0] | 0x80;

      // B. Cek Dot/Koma (Float handling)
      // Cek apakah karakter *berikutnya* adalah titik/koma
      if (str_idx + 1 < str_len &&
          (str[str_idx + 1] == '.' || str[str_idx + 1] == ',')) {
        seg_data |= 0x80; // Nyalakan bit DP (biasanya bit 7 atau 0x80)
        str_idx++;        // Lompati karakter titiknya agar tidak diproses lagi
      }

      str_idx++; // Pindah ke karakter utama berikutnya
    } else {
      // Jika string lebih pendek dari 8 digit, sisa digit dimatikan (spasi)
      seg_data = 0x00;
    }

    // Mapping ke Buffer DMA
    // Buffer index 1 correspond to Addr 00H (7-Seg 1)
    // Buffer index 2 correspond to Addr 01H (LED 1)
    dma_buffer[1 + (i * 2)] = seg_data;            // 7-Segment
    dma_buffer[1 + (i * 2) + 1] = (leds >> i) & 1; // LED
  }

  // 3. Mulai Transaksi DMA
  is_transferring = 1;
  STB_LOW(); // Tarik STB Low sebelum DMA mulai

  // Kirim 17 byte (1 Cmd + 16 Data)
  if (HAL_SPI_Transmit_DMA(&hspi2, dma_buffer, 17) != HAL_OK) {
    is_transferring = 0;
    STB_HIGH(); // Error recovery
  }
}

// --- Callback saat DMA Selesai ---
// Fungsi ini dipanggil otomatis oleh HAL Interrupt Handler
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
  if (hspi->Instance == SPI2) {
    STB_HIGH(); // Kembalikan STB ke High setelah semua data terkirim
    is_transferring = 0;
  }
}

// --- Fungsi Baca Tombol (Tetap Blocking) ---
// Baca tombol jarang dilakukan dan butuh respon cepat untuk logic,
// jadi blocking 4 byte (sangat cepat) biasanya lebih aman daripada DMA RX yang
// kompleks.
uint8_t TM1638_ReadButtons(void) {
  // ... Gunakan kode blocking read buttons yang sebelumnya ...
  // Karena membaca hanya 5 byte total, overhead DMA setup mungkin lebih lambat
  // daripada blocking SPI di 2-4MHz.
  uint8_t buttons[4] = {0};
  uint8_t cmd = 0x42;
  uint8_t final_key = 0;

  STB_LOW();
  HAL_SPI_Transmit(&hspi2, &cmd, 1, 10);

  // Ganti arah ke Input (Trik Half Duplex)
  __HAL_SPI_DISABLE(&hspi2);
  SPI2->CR1 &= ~SPI_CR1_BIDIOE;
  __HAL_SPI_ENABLE(&hspi2);

  HAL_SPI_Receive(&hspi2, buttons, 4, 10);

  __HAL_SPI_DISABLE(&hspi2);
  SPI2->CR1 |= SPI_CR1_BIDIOE;
  __HAL_SPI_ENABLE(&hspi2);
  STB_HIGH();

  // Decoding logic (sama seperti sebelumnya)
  final_key |= (buttons[0] & 0x01) ? 0x01 : 0;
  final_key |= (buttons[1] & 0x01) ? 0x02 : 0;
  final_key |= (buttons[2] & 0x01) ? 0x04 : 0;
  final_key |= (buttons[3] & 0x01) ? 0x08 : 0;
  final_key |= (buttons[0] & 0x10) ? 0x10 : 0;
  final_key |= (buttons[1] & 0x10) ? 0x20 : 0;
  final_key |= (buttons[2] & 0x10) ? 0x40 : 0;
  final_key |= (buttons[3] & 0x10) ? 0x80 : 0;

  return final_key;
}

// Dummy functions agar tidak error saat compile jika masih dipanggil
void TM1638_Clear(void) { TM1638_SendDMA("        ", 0); }

void TM1638_DisplayString(const char *str) {
  // Kita asumsikan LEDs mati kalau panggil ini, atau simpan state global LEDs
  TM1638_SendDMA(str, 0);
}

void TM1638_SetLEDs(uint8_t data) {
  // Perlu logic tambahan jika ingin update LED tanpa hapus teks
  // Tapi untuk simplifikasi DMA, kirim ulang semua paket lebih mudah.
}
