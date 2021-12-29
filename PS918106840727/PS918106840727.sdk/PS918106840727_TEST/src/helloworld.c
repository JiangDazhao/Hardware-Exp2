//#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"

//设置 MIO引脚地址
#define MIO_PIN_07		(*(volatile unsigned int *)0xF800071C)
#define MIO_PIN_50		(*(volatile unsigned int *)0xF80007C8)
#define MIO_PIN_51		(*(volatile unsigned int *)0xF80007CC)

//设置 GPIO端口方向寄存器地址
#define DIRM_0			(*(volatile unsigned int *)0xE000A204)
#define DIRM_1			(*(volatile unsigned int *)0xE000A244)
#define DIRM_2			(*(volatile unsigned int *)0xE000A284)
#define DIRM_3			(*(volatile unsigned int *)0xE000A2C4)
//设置 GPIO端口输出使能寄存器地址
#define OEN_0			(*(volatile unsigned int *)0xE000A208)
#define OEN_1			(*(volatile unsigned int *)0xE000A248)
#define OEN_2			(*(volatile unsigned int *)0xE000A288)
#define OEN_3			(*(volatile unsigned int *)0xE000A2C8)
//设置 GPIO端口输出寄存器地址
#define DATA_0			(*(volatile unsigned int *)0xE000A040)
#define DATA_1			(*(volatile unsigned int *)0xE000A044)
#define DATA_2			(*(volatile unsigned int *)0xE000A048)
#define DATA_3			(*(volatile unsigned int *)0xE000A04C)
//设置 GPIO端口输入寄存器地址
#define DATA_0_RO		(*(volatile unsigned int *)0xE000A060)
#define DATA_1_RO		(*(volatile unsigned int *)0xE000A064)
#define DATA_2_RO		(*(volatile unsigned int *)0xE000A068)
#define DATA_3_RO		(*(volatile unsigned int *)0xE000A06C)

//设置 UART1引脚地址的宏定义
#define rMIO_PIN_48		(*(volatile unsigned long*)0xF80007C0)
#define rMIO_PIN_49 	(*(volatile unsigned long*)0xF80007C4)
#define rUART_CLK_CTRL 	(*(volatile unsigned long*)0xF8000154)
#define rControl_reg0 	(*(volatile unsigned long*)0xE0001000)
#define rMode_reg0 		(*(volatile unsigned long*)0xE0001004)
//设置 UART1端口波特率等参数地址寄存器的宏定义
#define rBaud_rate_gen_reg0 (*(volatile unsigned long*)0xE0001018)
#define rBaud_rate_divider_reg0 (*(volatile unsigned long*)0xE0001034)
#define rTx_Rx_FIFO0 (*(volatile unsigned long*)0xE0001030)
#define rChannel_sts_reg0 (*(volatile unsigned long*)0xE000102C)

void send_Char_9(unsigned char modbus[]);				//9字节串口发送函数
void send_Char(unsigned char data);						//串口发送函数，一次一个字节
void RS232_Init();										//串口初始化函数
void delay(int i,int n,int m);							//延时函数

int getId();			//获取选中的机械臂
int getDir();			//要转动的方向
int getBoxTrans();		//获取导轨和传送带指令

void turnArm(int arm_id,int dir,int spd);		//arm_id机械臂朝着对应的方向按档位转动一次
void moveArm(int dir,int spd);					//机械臂沿着导轨左右移动
void swTrans(int fg);							//开启或者关闭传送带
void proBox(int fg);							//在二号传送带放一个箱子
void catch(int fg);								//控制吸盘
void initArm();
void endArm();
void GoBlue();
void GoRed();
void GoGreen();
void autoExColor();
//9个字节数据的发送函数
void send_Char_9(unsigned char modbus[])
{
	int i;
	char data;
	for(i=0;i<9;i++){
		data=modbus[i];
		send_Char(data);
		delay(100,10,10);		//延时
	}
}

//单个字节数据的发送函数
void send_Char(unsigned char data)
{
	//先判断发送FIFO是非满，若满则循环查询，直到发送FIFO不满
     while((rChannel_sts_reg0&0x10)==0x10);
     //发送一个字节的数据
     rTx_Rx_FIFO0=data;
}

//接收函数
unsigned rec_Char(void)
{
	char data;
	//先判断接受FIFO是非空，若空则循环查询，直到接收FIFO不空
	while((rChannel_sts_reg0&0x2)==0x2);
	data= rTx_Rx_FIFO0;
	return data;
}

//UART1的初始化函数
void RS232_Init()
{
     rMIO_PIN_48=0x000026E0;
     rMIO_PIN_49=0x000026E0;
     rUART_CLK_CTRL=0x00001402;
     rControl_reg0=0x00000017;
     rMode_reg0=0x00000020;
     rBaud_rate_gen_reg0=62;
     rBaud_rate_divider_reg0=6;
}

//延时函数
void delay(int n,int m,int p)
{
	 int i,j,k;
	 for(i=1;i<=n;i++){
		 for(j=1;j<=m;j++){
			 for(k=1;k<=p;k++){

			 }
		 }
	 }
}


int direction=0,arm_idx=-1,speed=0,btn=0,track=0;   //方向,机械臂轴下标,转速,按钮,导轨
int CurPos[6]={0,0,0,0,0,0}; 	//轴的角度

//复位函数
void reset(){
	for(int i=0;i<6;i++){
		if(CurPos[i]!=0){
			if(CurPos[i]>0){
				while(CurPos[i]!=0){
					CurPos[i]=(CurPos[i]-5)%360;
					turnArm(i,-1,4);
				}
			 }
			 if(CurPos[i]<0){
				 while(CurPos[i]!=0){
					CurPos[i]=(CurPos[i]+5)%360;
					turnArm(i,1,4);
				}
			 }
		}
	}

	//复位导轨
	if(track!=0){
		if(track>0){
			moveArm(2,2);
			track-=2;
		}
		else{
			moveArm(1,2);
			track+=2;
		}
	}
}

void singleStep(){
	direction=getDir(); 	//获得机械臂方向
	arm_idx=getId(); 			//获得机械臂序号
	delay(100,10,10);
	btn=getBoxTrans();
	if(direction!=0){
		DATA_0 = DATA_0 | 0x00000080;		//LED9指示灯亮
		delay(100,10,10);
		while(getDir()!=0);
		DATA_0 = DATA_0 & 0xFFFFFF7F;		//LED9指示灯灭
		CurPos[arm_idx]+=direction*5;
		turnArm(arm_idx,direction,4);
	}
	if(btn!=0){
		DATA_0 = DATA_0 | 0x00000080;		//LED9指示灯亮
		delay(100,10,10);
		while(getBoxTrans()!=0);
		DATA_0 = DATA_0 & 0xFFFFFF7F;		//LED9指示灯灭
		switch(btn){
		case 1:
			moveArm(1,2);track+=2;//导轨左移
			break;
		case 2:
			moveArm(2,2);track-=2;//导轨右移
			break;
		case 4:
			swTrans(1);//传送带开
			break;
		case 8:
			swTrans(2);//传送带关
			break;
		case 16:
			proBox(1);//出箱子
			break;
		}
	}
}

//根据颜色执行动作
void autoExColor(){
	initArm();
	for(int count=0;count<10;count++){
		swTrans(1);
		if(count)for(int j=0;j<25;j++){
			delay(1000,500,10);
		}
		proBox(1);
		RS232_Init();
		delay(1000,100,50);
		unsigned char color = rec_Char();
		if(color=='B'){
			GoBlue();
		}else if(color=='G'){
			GoGreen();
		}else if(color=='R'){
			GoRed();
		}
	}
	endArm();
}

void initArm(){
	for(int i=1;i<=18;i++){
		turnArm(0,1,4);
	}
	delay(100,100,50);
	for(int i=1;i<=5;i++){
		turnArm(1,1,4);

	}delay(100,100,50);
	for(int i=1;i<=15;i++){
		turnArm(2,-1,4);

	}delay(100,100,50);
	for(int i=1;i<=28;i++){
		turnArm(4,1,4);

	}delay(100,100,50);
}
void endArm(){
	for(int i=1;i<=18;i++){turnArm(0,-1,4);}
	delay(100,100,50);
	for(int i=1;i<=5;i++){turnArm(1,-1,4);}
	delay(100,100,50);
	for(int i=1;i<=15;i++){turnArm(2,1,4);}
	delay(100,100,50);
	for(int i=1;i<=28;i++){turnArm(4,-1,4);}
	delay(100,100,50);
}

void GoBlue(){
	catch(1);
	for(int i=1;i<=36;i++){
		turnArm(0,-1,4);

	}delay(100,100,50);
	for(int i=1;i<=20;i++){
		moveArm(1,3);

	}delay(100,100,50);
	catch(2);
	for(int i=1;i<=36;i++){
		turnArm(0,1,4);

	}delay(100,100,50);
	for(int i=1;i<=20;i++){
		moveArm(2,3);

	}delay(100,100,50);
}

void GoRed(){
	catch(1);
	for(int i=1;i<=36;i++){
		turnArm(0,-1,4);
	}delay(100,100,50);
	for(int i=1;i<=20;i++){
		moveArm(2,3);

	}delay(100,100,50);
	catch(2);
	for(int i=1;i<=36;i++){
		turnArm(0,1,4);

	}delay(100,100,50);
	for(int i=1;i<=20;i++){
		moveArm(1,3);

	}delay(100,100,50);
}
void GoGreen(){
	catch(1);
	for(int i=1;i<=36;i++){
		turnArm(0,-1,4);

	}delay(100,100,50);
	catch(2);
	for(int i=1;i<=36;i++){
		turnArm(0,1,4);

	}delay(100,100,50);
}


int main()
{

	init_platform();
	u32 flag;		//变量flag用于记录SW0~SW7按键按下信息;

	//注：下面MIO引脚和EMIO引脚的序号是统一编号的，MIO序号为0~31及32~53，EMIO序号为54~85及86~117
	//配置及初始化MIO07引脚的相关寄存器，MIO07作为LED灯控制的输出引脚
	MIO_PIN_07 = 0x00003600;
	DIRM_0 = DIRM_0|0x00000080;
	OEN_0 = OEN_0|0x00000080;
	//配置及初始化MIO50、MIO51引脚的相关寄存器，MIO50、MIO51作为按键输入引脚
	MIO_PIN_50 = 0x00003600;
	MIO_PIN_51 = 0x00003600;
	DIRM_1 = DIRM_1 & 0xFFF3FFFF;
	//初始化EMIO54~EMIO58的引脚，它们对应BTNU、BTND、BTNL、BTNR、BTNC按键，输入
	DIRM_2 = DIRM_2 & 0xFFFFFFE0;
	//初始化EMIO59~EMIO66的引脚，它们对应SW7~SW0拨动开关，输入
	DIRM_2 = DIRM_2 & 0xFFFFE01F;
	//初始化EMIO67~EMIO74的引脚，它们对应LED7~LED0，输出
	DIRM_2 = DIRM_2|0x001FE000;
	OEN_2 = OEN_2|0x001FE000;

	//初始化UART1
	RS232_Init();

	int fgauto=1;
    while(1){
    	//读模式信息，即读SW7、SW6的输入信息
    	flag = DATA_2_RO&0x00000060;//【EMIO 59-60】
        switch(flag){
        case 0x00:					//复位模式
        	DATA_2 = DATA_2&0xFFE01FFF;		//模式指示灯LED7~LED0灭
        	reset();
        	fgauto=1;
    		delay(100,10,10);
        	break;
        case 0x20:					//手动控制模式
        	DATA_2 = (DATA_2|0x00002000)&0xFFFFBFFF;	//指示灯LED7亮、LED6灭【EMIO 67 68】
        	singleStep();
			break;
        case 0x40:					//自动控制模式
        	DATA_2 = (DATA_2|0x00004000)&0xFFFF7FFF;	//LED7灭、LED6亮
        	if(fgauto){
				autoExColor();
				delay(1000,100,500);
        	}
        	fgauto=0;
        	break;
        case 0x60:					//机械臂示教模式（该模式暂不实现）
        	DATA_2 = DATA_2|0x00006000;					//LED7亮、LED6亮
        	break;
        }
    }
    return 0;
}


//获取导轨和传送带指令
int getBoxTrans(){
	int flag=DATA_2_RO & 0x0000001f; //0-4位，对应轨道左右运动、传送带开关、出箱子【EMIO 54-58】
	return flag;
}

//获取选中的机械臂
int getId(){
   u32 flag;
   flag=DATA_2_RO & 0x00001f80; //7-12位对应6个编号【EMIO 61-66】
   switch(flag){				//单次只能选择一个轴进行转动
   	   case 0x00000080:
   		   return 5;
   	       break;
   	   case 0x00000100:
	       return 4;
	       break;
   	   case 0x00000200:
   		   return 3;
   		   break;
   	   case 0x00000400:
   		   return 2;
   		   break;
   	   case 0x00000800:
   		   return 1;
   		   break;
   	   case 0x00001000:
   		   return 0;
   }
   return -1;
}

//要转动的方向
int getDir(){
   int flag=DATA_1_RO & 0x000c0000;
   if(flag==0x00040000) return -1; 		//MIO50逆
   else if(flag==0x00080000) return 1;  //MIO51顺
   return 0;
}

//arm_id机械臂朝着对应的方向按档位转动一次
void turnArm(int arm_id,int dir,int spd){
	unsigned char modbus_com[9];
	modbus_com[0]='#';	//起始符，固定为#
	modbus_com[1]='2';
	modbus_com[2]='0';
	modbus_com[3]='0';
	modbus_com[4]='0';
	modbus_com[5]='0';
	modbus_com[6]='0';
	modbus_com[7]='0';
	modbus_com[8]='0';

	if(arm_id==-1 || dir==0)return;
	else if(dir==1)modbus_com[arm_id+2]=0x30+spd;	//顺时针
	else if(dir==-1)modbus_com[arm_id+2]=0x34+spd;	//逆时针

	send_Char_9(modbus_com);
}

//机械臂沿着导轨左右移动
void moveArm(int dir,int spd){
	unsigned char modbus_com[9];
	modbus_com[0]='#';
	modbus_com[1]='2';
	modbus_com[2]='0';
	modbus_com[3]='0';
	modbus_com[4]='0';
	modbus_com[5]='0';
	modbus_com[6]='0';
	modbus_com[7]='0';
	modbus_com[8]='0';

	if(dir==0)return;
	else if(dir==1)modbus_com[8]=0x30+spd;	//轨道左移
	else if(dir==2)modbus_com[8]=0x33+spd;	//轨道右移

	send_Char_9(modbus_com);
}

//开启或者关闭传送带
void swTrans(int fg){
	unsigned char modbus_com[9];
	modbus_com[0]='#';
	modbus_com[1]='6';
	modbus_com[2]='0';
	modbus_com[3]='0';
	modbus_com[4]='0';
	modbus_com[5]='0';
	modbus_com[6]='0';
	modbus_com[7]='0';
	modbus_com[8]='0';
	modbus_com[3]=0x30+fg; 		//fg==1开 2关

	send_Char_9(modbus_com);
}

//在二号传送带放一个箱子
void proBox(int fg){
	unsigned char modbus_com[9];
	modbus_com[0]='#';
	modbus_com[1]='4';
	modbus_com[2]='0';
	modbus_com[3]='0';
	modbus_com[4]='0';
	modbus_com[5]='0';
	modbus_com[6]='0';
	modbus_com[7]='0';
	modbus_com[8]='0';
	modbus_com[3]=0x30+fg;		//fg==0不出箱子   1出箱子
	send_Char_9(modbus_com);

}

//控制吸盘
void catch(int fg){
	unsigned char modbus_com[9];
	modbus_com[0]='#';
	modbus_com[1]='5';
	modbus_com[2]='0';
	modbus_com[3]='0';
	modbus_com[4]='0';
	modbus_com[5]='0';
	modbus_com[6]='0';
	modbus_com[7]='0';
	modbus_com[8]='0';
	modbus_com[3]=0x30+fg;		//fg==0不动作   1吸  2放
	send_Char_9(modbus_com);
}
