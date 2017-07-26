#include "ioCC2530.h"
#include <stdio.h>
#include <string.h>

#define LED1 P1_0
#define LED2 P1_1

char rf_rx_buf[128];    //rf������
char serial_rxbuf[128]; //���ڽ��ܻ�����
int serial_rxpos = 0;
int serial_rxlen = 0;
char is_serial_receive = 0;

void uart0_init();
void uart0_sendbuf(char *pbuf, int len);
void uart0_flush_rxbuf();       //���棺����������

void timer1_init();
void timer1_disable();
void timer1_enable();

void rf_send(char *pbuf, int len);
void rf_receive_isr();

void uart0_init()
{
  PERCFG = 0x00;        //UART0ѡ��λ��0 TX@P0.3 RX@P0.2
  P0SEL |= 0x0C;        //P0.3 P0.2ѡ�����蹦�ܣ��ڶ�λ����λΪ1
  U0CSR |= 0xC0;        //UARTģʽ��������ʹ��
  U0GCR |= 11;          //����� U0GCR �� U0BAUD�趨������Ϊ115200
  U0BAUD = 216;
  
  UTX0IF = 1;           //�ж�δ��
  URX0IE = 1;           //USART0 RX�����ж�ʹ��
}

void uart0_flush_rxbuf()
{
  serial_rxpos = 0;
  serial_rxlen = 0;
}

void timer1_init()
{
  T1IE = 1;             //IEN1@BIT1 ʹ�ܶ�ʱ��1�ж�
  
  T1CTL = 0x0C;         //@DIV��Ƶϵ�� 128 @MODE��ͣ����
  T1CCTL0 = 0x44;       //@IMͨ��0�ж�ʹ�� @MODE�Ƚ�ƥ��ģʽ
  T1STAT = 0x00;
  
  T1IE = 1;             //IEN1@BIT1 ʹ�ܶ�ʱ��1�ж�
  
  T1CC0L = 250;         //�������Ϊ2ms
  T1CC0H = 0;
}

void timer1_disable()
{
  T1CTL &= ~(1 << 1);
}

void timer1_enable()
{
  T1CTL |= (1 << 1);    //TIMER1@MODE 10��ģ����0x0000��T1CC0��������
  T1STAT = 0x00;        //����жϱ�־λ
  T1CNTH = 0;           //���¿�ʼ����
  T1CNTL = 0;
}

void rf_init()
{
  TXPOWER       = 0xF5; //���书��Ϊ4.5dBm�����
  CCACTRL0      = 0xF8; //�Ƽ�ֵ smartRF������� CCA:����ͨ������
  
  FRMFILT0      = 0x0C; //��ֹ���ֹ��ˣ��������������ݰ�
  
  FSCAL1        = 0x00; //�Ƽ�ֵ smartRF�������
  TXFILTCFG     = 0x09;
  AGCCTRL1      =  0x15;
  AGCCTRL2      =  0xFE;

  TXFILTCFG     = 0x09; // �Ƽ�ֵ smartRF�������
  
  FREQCTRL      = 0x0B; // ѡ��ͨ��11

  RFIRQM0       |= (1<<6);      // ʹ��RF���ݰ������ж�
  IEN2          |= (1<<0);      // ʹ��RF�ж�

  RFST          = 0xED; // ���RF���ջ����� ISFLUSHRX
  RFST          = 0xE3; // RF����ʹ�� ISRXON
}

void rf_send(char *pbuf , int len)      //��Ƶ��������
{
  RFST = 0xE3;          //RF����ʹ�� ISRXON
  //�ȴ�����״̬����Ծ������û�н��ܵ�SFD
  while( FSMSTAT1 & (( 1<<1 ) | ( 1<<5 )));
  
  RFIRQM0 &= ~( 1<<6 ); //��ֹ�������ݰ��ж�
  IEN2 &= ~( 1<<0 );    //���RFȫ���ж�
  
  RFST = 0xEE;          //������ͻ����� ISFLUSHTX
  RFIRQF1 = ~( 1<<1 );  //���������ɱ�־
  
  //��仺��������������Ҫ����2�ֽڣ�CRCУ���Զ����
  RFD = len + 2;
  
  printf("RFD = %3c\r\n",RFD);
  printf("RFD = %3d\r\n",RFD);
  
  for (int i = 0; i < len; i++)
  {
      RFD = *pbuf++;    //дRFD���൱��д���ͻ�����
  }
  
  RFST = 0xE9;          //�������ݰ� ISTXON
  while (!(RFIRQF1 & ( 1<<1 )));        //�ȴ��������
  RFIRQF1 = ~( 1<<1 );  //���������ɱ�־λ
  
  RFIRQM0 |= ( 1<<6 );  //RX�����ж�
  IEN2 |= ( 1<<0 );     //RFһ���ж�ʹ��
  
}

void rf_receive_isr()   //�������ݰ�
{
  int rf_rx_len = 0;
  int rssi = 0;
  char crc_ok = 0;
  
  rf_rx_len = RFD - 2;  //����ȥ��2�ֽڵĸ��ӽ��
  rf_rx_len &= 0x7F;    //���Ȳ��ܳ���128
  for (int i = 0; i < rf_rx_len; i++)
  {
    rf_rx_buf[i] = RFD;         //������ȡ������������
  }
  
  rssi = RFD - 73;      //��ȡRSSI��� ����������
  crc_ok = RFD;         //ȥ��crcУ���� BIT7
  
  RFST = 0xED;          //������ջ����� 
  if( crc_ok & 0x80)
  {
    uart0_sendbuf( rf_rx_buf , rf_rx_len);      //���ڷ���
  }
  else
  {
    printf("\r\nCRC Error\r\n");
  }
}

void main(void)
{
  P1DIR |= ( 1<<0 )| ( 1<<1 );          // P1.0���
  LED1 = 0;
  LED2 = 0;
  
  EA = 0;                       //��ʱ�ر�ȫ���ж�
  
  SLEEPCMD &= ~0x04;            //����ϵͳʱ��Ϊ32Mhz
  while( !(SLEEPSTA & 0x40));
  CLKCONCMD &= ~0x47;
  SLEEPCMD = 0x04;
  
  uart0_init();         //���ڳ�ʼ��
  timer1_init();        //��ʱ����ʼ�� 2ms �Ƚ�ƥ��
  rf_init();            //RF��ʼ�� ��֡����
  
  EA = 1;               //ʹ��ȫ���ж�
  
  printf("The system is running....\r\n");
  
  while(1)
  {
    if ( is_serial_receive)     //�յ��������ݰ�
    {
      is_serial_receive = 0;    //�����־λ
      LED1 ^= 1;
      rf_send(serial_rxbuf , serial_rxlen);     //ֱ��ת����������
      uart0_flush_rxbuf();      //������ڷ�������
    }
  }
}
      
int putchar( int c)
{
  while( !UTX0IF);
  UTX0IF = 0;
  U0DBUF = c;
  return c;
}

void uart0_sendbuf(char *pbuf, int len)
{
  for( int i = 0; i < len ; i++ )
  {
    while( !UTX0IF);
    UTX0IF = 0;
    U0DBUF = *pbuf;
    pbuf++;
  }
}

#pragma vector=URX0_VECTOR
__interrupt void UART0_ISR(void)
{
    URX0IF = 0;                                   // ��������жϱ�־
    serial_rxbuf[serial_rxpos] = U0DBUF;          // ��仺����
    serial_rxpos++;
    serial_rxlen++;

    timer1_enable();                              // ��ʱ�����¿�ʼ����
}


#pragma vector=T1_VECTOR
__interrupt void Timer1_ISR(void)
{
    T1STAT &= ~( 1<< 0);                          // �����ʱ��T1ͨ��0�жϱ�־

    is_serial_receive = 1;                        // �������ݵ���
    timer1_disable();
}

#pragma vector=RF_VECTOR
__interrupt void rf_isr(void)
{
    LED2 ^= 1;                                    // LED1��ת ��ʾ����
    EA = 0;

    // ���յ�һ�����������ݰ�
    if (RFIRQF0 & ( 1<<6 ))
    {
        rf_receive_isr();                           // ���ý����жϴ�����

        S1CON = 0;                                  // ���RF�жϱ�־
        RFIRQF0 &= ~(1<<6);                         // ���RF����������ݰ��ж�
    }
    EA = 1;
}

  
  
           
  
  
  



