/*
 * HMI.c
 *
 *  Created on: 2019��8��7��
 *      Author: zengwangfa
 *      Notes:  ������ͨ��
 */

#include "HMI.h"
#include "DataType.h"
#include <rtthread.h>
#include "uart.h"
#include "DataProcess.h"
/*---------------------- Constant / Macro Definitions -----------------------*/

#define HMI_LEN 5

/*----------------------- Variable Declarations -----------------------------*/

uint8 hmi_data_ok = 0;
int HMI_Write_Flag = 0; //д������Flag
int HMI_Status_Flag = 0;//������ �趨״̬��־λ ������ 1��or������2��
int HMI_Page_Number = 0;//���������͵�У׼  ֽ����
int HMI_Work_Button = 0; /* ��� ȷ�� ��ť*/
//76 61 30 2E 76 61 6C 3D 31 ff ff ff
//76 61 30 2E 76 61 6C 3D 31 ff ff ff   va0.val=1
uint8 him_uart_cmd[12] = {0x76,0x61,0x30,0x2E,0x76,0x61,0x6C,0x3D,0x31,0xFF,0xFF,0xFF};    // 01 д��ɹ�  02 д��ʧ��   03 ����д��... 04 ��ʾhmi����

//76 61 31 2E 76 61 6C 3D 31 30 30 ff ff ff  va1.val=100
uint8 him_uart_nmber_cmd[14] = {0x76,0x61,0x31,0x2E,0x76,0x61,0x6C,0x3D,0x31,0x30,0x30,0xff,0xff,0xff};

//76 61 32 2E 76 61 6C 3D 31 ff ff ff   va2.val=1
uint8 him_uart_short_cmd[12] = {0x76,0x61,0x32,0x2E,0x76,0x61,0x6C,0x3D,0x30,0xff,0xff,0xff};


uint8 him_ret_status = 0;
uint8 hmi_data[10] = {0};
float FDC2214_Page_Data_Top[100]    = {0};
float FDC2214_Page_Data_Bottom[100] = {0};
float FDC2214_Page_Data_Single[100] = {0};

extern int ShortFlag;        //��·��־λ
/*----------------------- Function Implement --------------------------------*/

/* ���͸������� д���״̬
01:д��ɹ�
02:д��ʧ��
03:����д��
04:��ʾhmi����
*/
void uart_send_hmi_writer_status(uint8 *cmd)//���͸� hmiд���״̬
{

		*cmd += 0x30; //����+0x30  ת�ɶ�Ӧ��ASCII ��Ӧд��
		rt_device_write(focus_uart_device, 0,him_uart_cmd	, sizeof(him_uart_cmd));
		*cmd = 0; //��������
}



void uart_send_hmi_paper_numer(uint8 N_number)  //���͸�hmi ֽ������
{ 	


		N_number = N_number/100%10;
		him_uart_nmber_cmd[8] =N_number + 0x30;  //��λ

		N_number = N_number/10%10;
		him_uart_nmber_cmd[9] = N_number + 0x30; //ʮλ
		
		N_number = N_number/10%1;
		him_uart_nmber_cmd[10] = N_number + 0x30;//��λ
	
		rt_device_write(focus_uart_device, 0,him_uart_nmber_cmd	, sizeof(him_uart_nmber_cmd));
			
}


void uart_send_hmi_is_short(void)  //���͸�hmi �Ƿ��·
{ 	
		if(1 == Paper.ShortStatus)//����·
		{
				him_uart_short_cmd[8] = 0x31;
		}
		else if(2 == Paper.ShortStatus){
				him_uart_short_cmd[8] = 0x32;
		}
		else{
				him_uart_short_cmd[8] = 0x30;				
		}

		
		rt_device_write(focus_uart_device, 0,him_uart_short_cmd	, sizeof(him_uart_short_cmd));//��HMI���Ͷ�·��Ϣ
			
}


/* ���͸������� д���״̬
01:д��ɹ�
02:д��ʧ��
03:����д��
04:��ʾhmi����
*/
void FDC2214_Data_Adjust(void)//����У׼ �洢
{
		rt_thread_mdelay(1000);
	
		Paper.Status = 0x03; //����д��
	
		FDC2214_Page_Data_Top   [HMI_Page_Number] = get_top_capacity();   //���� ��Ӧҳ ����ֵ����
		FDC2214_Page_Data_Bottom[HMI_Page_Number] = get_bottom_capacity();//�װ� ��Ӧҳ ����ֵ����
	
		FDC2214_Page_Data_Single[HMI_Page_Number] = get_single_capacity();//���� ��Ӧҳ ����ֵ����
	
		
		rt_kprintf("pagenum:%d,cap:%f\n",HMI_Page_Number,FDC2214_Page_Data_Single[HMI_Page_Number]);
		rt_thread_mdelay(1000);
	
		Paper.Status = 0x01; //д��ɹ�

}

/**
  * @brief  HMI_Data_Analysis(�������������ݽ���)
  * @param  �����ַ����� uint8 Data
  * @retval None
  * @notice �ӵ��ĸ��ֽڿ�ʼΪ�����ֽ�
  */
void HMI_Data_Analysis(uint8 Data) //�������ݽ���
{

		static uint8 i = 0;	   		  //
		static uint8 RxCheck = 0;	  //βУ����
		static uint8 RxCount = 0;	  //���ռ���
	
		hmi_data[RxCount++] = Data;	//���յ������ݴ��뻺������
	
	
		if(RxCount <= (HMI_LEN+4)){ //�������ݳ���δ������ͷ�Ͱ���3���ֽ�,+4)  
				if(hmi_data[0] == 0xAA){ //���յ���ͷ0xAA
						if(RxCount > 3){
								if(hmi_data[1] == 0x55){ //���յ���ͷ0x55
										if(RxCount >= hmi_data[2]+4){ //���յ����ݰ�����λ����ʼ�ж�ʲôʱ��ʼ����У��
												for(i = 0;i <= (RxCount-2);i++){ //�ۼӺ�У��
														RxCheck += hmi_data[i];
												}
			
												if(RxCheck == hmi_data[RxCount-1]){
														hmi_data_ok = 1; //�������ݰ��ɹ�
												}
												else {hmi_data_ok = 0;}
												
												RxCheck = 0; //�����������
												RxCount = 0;	
										}
								}
								else {hmi_data_ok = 0;RxCount = 0;hmi_data_ok = 0;} //���ղ��ɹ�����
						}
				}
				else {hmi_data_ok = 0;RxCount = 0;hmi_data_ok = 0;} //���ղ��ɹ�����
		}
		else {hmi_data_ok = 0;RxCount = 0;hmi_data_ok = 0;} //���ղ��ɹ�����

		
		if(1 == hmi_data_ok){
				HMI_Status_Flag = hmi_data[3];//��ȡ ����ģʽλ
				
				//rt_kprintf("HMI_Status_Flag:%d\n",HMI_Status_Flag);
				if(hmi_data[4] == 0xFF){//����� 0xFF  ���ж�Ϊ��ť����
						HMI_Work_Button = 1; //����ģʽ�� ��ť
				}
				else{		
						HMI_Write_Flag = 1; 	//д��ҳ��״̬
						HMI_Page_Number = hmi_data[4];//��ȡ У׼ҳ��
						HMI_Work_Button = 0;
				}
		}
		else{

		}
		hmi_data_ok = 0;
}


uint32 get_set_page_number(void)
{
		return HMI_Page_Number;
}

uint32 get_set_status(void)
{
		return HMI_Status_Flag;
}




