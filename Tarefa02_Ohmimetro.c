//Tarefa 02 - Conceitos de Eletrônica - Ohmímetro 
//Aluna: Maryana Souza Silveira
//Residência Tecnológica em Sistemas Embarcados - EmbarcaTech - CEPEDI

#include <stdio.h>                  //Biblioteca de entrada/saída padrão
#include <math.h>                   //Biblioteca de funções matemáticas
#include "pico/stdlib.h"            //Biblioteca padrão da Raspberry Pi Pico
#include "hardware/adc.h"           //Biblioteca do conversor analógico/digital
#include "hardware/i2c.h"           //Biblioteca de controle do protocolo I2C
#include "hardware/pio.h"           //Biblioteca para controle de PIO
#include "lib/ssd1306.h"            //Biblioteca para controle do display SSD1306
#include "lib/font.h"               //Biblioteca de fontes para o display SSD1306
#include "Tarefa02_Ohmimetro.pio.h" //Código do programa para controle de LEDs WS2812

#define I2C_PORT i2c1               //Define o barramento I2C
#define I2C_SDA 14                  //Define o pino SDA
#define I2C_SCL 15                  //Define o pino SCL
#define endereco 0x3C               //Endereço do display
#define ADC_PIN 28                  //GPIO para o ohmímetro

const uint16_t R_known = 10000;       //Resistor conhecido de 10k ohm
float R_x = 0.0;                      //Resistor desconhecido
const uint16_t ADC_RESOLUTION = 4095; //Resolução do ADC (12 bits)

ssd1306_t ssd;                       //Inicializa a estrutura do display

const uint8_t Led_Array[25] = {     //Status da matriz 5x5
  4, 4, 4, 4, 4,                    //Linha com cor da tolerância do resistor (neste caso, sempre 5%)
  0, 0, 0, 0, 0,
  3, 3, 3, 3, 3,                    //Linha com cor do multiplicador do resistor
  2, 2, 2, 2, 2,                    //Linha com cor do 2º parâmetro do resistor
  1, 1, 1, 1, 1,                    //Linha com cor do 1º parâmetro do resistor
};

//Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
  reset_usb_boot(0, 0);
}

void initial_configs();                                                                 //Função para setar as configurações iniciais
uint32_t resist_E24 (float resist_found, uint16_t parameters_vector[3]);                //Função que retorna o resistor da série E24 mais próximo
void ssd1306_resistor(ssd1306_t *ssd, uint8_t x_display, uint8_t y_display);            //Função que desenha um resistor no display
void turn_on_leds(const uint8_t matriz[25], uint8_t num1, uint8_t num2, uint16_t mult); //Função para acender os leds da matriz 5x5
static inline void put_pixel(uint32_t pixel_grb);                                       //Função para atualizar um LED
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);                       //Função para converter RGB para uint32_t

int main(){
  initial_configs();       //Inicializa as configurações
  uint16_t parameters[3]; //Vetor para armazenar os parâmetros do resistor

  char Res_E24[5];        //Buffer para armazenar a string do resistor E24 mais próximo
  char Res_Found[5];      //Buffer para armazenar a string do resistor encontrado
  //Matriz para armazenar os nomes das cores
  char colors[12][10] = {"preto", "marrom", "vermelho", "laranja", "amarelo", "verde", "azul", "violeta", "cinza", "branco", "dourado", "prateado"};

  bool cor = true;        //Variável para controle do display
  while (true){
    adc_select_input(2); //Seleciona o ADC para eixo X. O pino 28 como entrada analógica

    float sum = 0.0f;    //Variável para armazenar a soma dos valores lidos
    for (int i = 0; i < 500; i++){ //Loop para realizar a soma
      sum += adc_read();
      sleep_ms(1);
    }
    float mean = sum / 500.0f; //Obtém a média a cada 500 valores lidos

    printf("Valor do ADC: %.0f\n", mean); //Apresenta o valor do ADC no monitor serial

    // Fórmula simplificada: R_desconhecido = R_conhecido * ADC_encontrado /(ADC_RESOLUTION - adc_encontrado)
    R_x = (R_known * mean) / (ADC_RESOLUTION - mean);     //Obtém o valor do resistor desconhecido

    uint32_t R_E24 = resist_E24(R_x, parameters);         //Obtém o valor do resistor da série E24 mais próximo e armazena seus parâmetros no vetor

    sprintf(Res_Found, "%1.0f", R_x);                     //Converte o float em string
    sprintf(Res_E24, "%d", R_E24);                        //Converte o inteiro em string

    uint8_t num1 = parameters[0];                         //Atribui a posição do 1º parâmetro na matriz de cores
    uint8_t num2 = parameters[1];                         //Atribui a posição do 2º parâmetro na matriz de cores
    uint16_t mult = (log10(parameters[2]));               //Atribui a posição do multiplicador no array de cores

    turn_on_leds(Led_Array, num1, num2, mult);            //Acende a matriz de leds de acordo com os parâmetros

    //Atualiza o conteúdo do display com animações
    ssd1306_fill(&ssd, !cor);                          //Limpa o display
    ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);      //Desenha um retângulo
    ssd1306_line(&ssd, 3, 37, 123, 37, cor);           //Desenha uma linha
    ssd1306_resistor(&ssd, 20, 6);                     //Desenha um resistor
    ssd1306_draw_string(&ssd, colors[num1], 50, 6);    //Apresenta o nome da cor do 1º parâmetro
    ssd1306_draw_string(&ssd, colors[num2], 50, 16);   //Apresenta o nome da cor do 2º parâmetro
    ssd1306_draw_string(&ssd, colors[mult], 50, 26);   //Apresenta o nome da cor do multiplicador
    ssd1306_draw_string(&ssd, "Resist.", 8, 41);       //Desenha uma string
    ssd1306_draw_string(&ssd, "R_E24", 70, 41);        //Desenha uma string
    ssd1306_line(&ssd, 65, 37, 65, 60, cor);           //Desenha uma linha vertical
    ssd1306_draw_string(&ssd, Res_Found, 8, 52);       //Desenha a string com o valor encontrado 
    ssd1306_draw_string(&ssd, Res_E24, 70, 52);        //Desenha a string com o resistor mais próximo
    ssd1306_send_data(&ssd);                           //Atualiza o display
    sleep_ms(700);
  }
}

void initial_configs(){           //Função para setar as configurações iniciais
  stdio_init_all();               //Inicializa a comunicação serial

  // Para ser utilizado o modo BOOTSEL com botão B
  gpio_init(botaoB);              //Inicializa o pino do botão B
  gpio_set_dir(botaoB, GPIO_IN);  //Define o pino do botão B como entrada
  gpio_pull_up(botaoB);           //Habilita o resistor de pull-up do botão B
  gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler); //Habilita a interrupção do botão B

  //Inicializa o PIO
  PIO pio = pio0; 
  int sm = 0; 
  uint offset = pio_add_program(pio, &ws2812_program);

  //Inicializa o programa para controle dos LEDs WS2812
  ws2812_program_init(pio, sm, offset, 7, 800000, false); //Inicializa o programa para controle dos LEDs WS2812

  i2c_init(I2C_PORT, 400 * 1000);                               //Inicializa o barramento I2C a 400kHz
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                    //Configura a função do pino SDA
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                    ///Configura a função do pino SCL
  gpio_pull_up(I2C_SDA);                                        //Habilita o pull-up do pino SDA
  gpio_pull_up(I2C_SCL);                                        //Habilita o pull-up do pino SCL
  ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
  ssd1306_config(&ssd);                                         // Configura o display
  ssd1306_send_data(&ssd);                                      // Envia os dados para o display

  // Limpa o display. O display inicia com todos os pixels apagados.
  ssd1306_fill(&ssd, false);
  ssd1306_send_data(&ssd);

  adc_init();                  //Inicializa o ADC
  adc_gpio_init(ADC_PIN);      //GPIO 28 como entrada analógica
}

uint32_t resist_E24 (float resist_found, uint16_t parameters_vector[3]){ //Função que retorna o resistor da série E24 mais próximo
  //Vetor com os valores da série E24 multiplicados por 10
  uint8_t series_E24[24]= {10, 11, 12, 13, 15, 
                       16, 18, 20, 22, 24, 
                       27, 30, 33, 36, 39, 
                       43, 47, 51, 56, 62,
                       68, 75, 82, 91};
  uint sub = 100000;              //Variável para verificar a diferença entre os resistores da série e o encontrado
  uint8_t E24 = 0;                //Variável para armazenar o resistor mais próximo
  uint16_t mult = 1;              //Variável para armazenar o valor do multiplicador

  for (int i = 1; i <= 100000; i *= 10) {
    for (int j = 0; j < 24; j++) {
      int resist_series = series_E24[j] * i;          //Multiplica o valor do resistor por cada multiplicador possível no loop
      int value = abs(resist_series - resist_found);  //Obtém o valor absoluto da diferença entre o resistor E24 e o encontrado
      if (value < sub) {                              //Caso o valor da diferença seja menor que o encontrado anteriormente
        sub = value;                                  //Substitui o valor da diferença
        E24 = series_E24[j];                          //Armazena o valor do resistor E24
        mult = i;                                     //Armazena o valor do multiplicador
      }
    }
  }
  parameters_vector[0] = E24 / 10;                    //Armazena no vetor o 1º parâmetro
  parameters_vector[1] = E24 % 10;                    //Armazena no vetor o 2º parâmetro
  parameters_vector[2] = mult;                        //Armazena no vetor multiplicador
  return E24 * mult;                                  //Retorna o valor do resistor mais próximo
}

void ssd1306_resistor(ssd1306_t *ssd, uint8_t x_display, uint8_t y_display){ //Função para desenhar um resistor no display
  static const uint8_t pattern[6][8] = {              //Matriz com bits representados por blocos 8x8 que formam a figura do resistor
    {0x00, 0x00, 0x00, 0xe0, 0xd0, 0xd0, 0xc8, 0xcf},
    {0xcf, 0xc8, 0xd0, 0xd0, 0xe0, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x80, 0xff, 0x36, 0x36, 0x36},
    {0x36, 0x36, 0x36, 0xff, 0x80, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x07, 0x07, 0x0b, 0x0b, 0xfb},
    {0xfb, 0x0b, 0x0b, 0x07, 0x07, 0x00, 0x00, 0x00}
  };
  
  for (uint8_t i = 0; i < 8; ++i) {                   //Varre cada coluna de um bloco 8x8
    for (uint8_t j = 0; j < 8; ++j) {                 //Varre cada linha da coluna atual
      for (uint8_t p = 0; p < 6; ++p) {               //Varre cada um dos 6 blocos
        uint8_t x_offset = (p % 2) * 8;               //Calcula o deslocamento horizontal
        uint8_t y_offset = (p / 2) * 8;               //Calcula o deslocamneto vertical
        //Desenha o pixel se o bit for igual a 1
        ssd1306_pixel(ssd, x_display + x_offset + i, y_display + y_offset + j, pattern[p][i] & (1 << j));
      }
    }
  }
}

void turn_on_leds(const uint8_t matriz[25], uint8_t num1, uint8_t num2, uint16_t mult){ //Função para acender os leds da matriz 5x5
  const uint32_t colors[12] = {     //Vetor de cores em formato rgb
    urgb_u32(0, 0, 0),              //0 - preto
    urgb_u32(25, 12, 3),            //1 - marrom
    urgb_u32(50, 0, 0),             //2 - vermelho
    urgb_u32(50, 15, 0),            //3 - laranja
    urgb_u32(50, 50, 0),            //4 - amarelo
    urgb_u32(0, 50, 0),             //5 - verde
    urgb_u32(0, 0, 50),             //6 - azul
    urgb_u32(20, 0, 40),            //7- violeta
    urgb_u32(11, 15, 11),           //8 - cinza
    urgb_u32(25, 25, 25),           //9 - branco
    urgb_u32(20, 10, 0),            //10 - dourado
    urgb_u32(17, 15, 13)            //11 - prateado
  };
  
  for (int i = 0; i < 25; i++) {                    //Acende de acordo com o número na matriz de leds
    switch (matriz[i]){ 
      case 1: put_pixel(colors[num1]); break;       //Led com a cor da primeira faixa
      case 2: put_pixel(colors[num2]); break;       //Led com a cor da segunda faixa
      case 3: put_pixel(colors[mult]); break;       //Led com a cor da terceira faixa
      case 4: put_pixel(colors[10]); break;         //Led com a cor da quarta faixa (neste caso, sempre dourado - 5%)
      default: put_pixel(urgb_u32(0, 0, 0)); break; //Mantém a quarta fileira apagada
    }
  }
}

static inline void put_pixel(uint32_t pixel_grb){ //Função para atualizar um LED
  pio_sm_put_blocking(pio0, 0, pixel_grb << 8u); //Atualiza o LED com a cor fornecida
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b){ //Função para converter RGB para uint32_t
  return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b); //Retorna a cor em formato uint32_t 
}