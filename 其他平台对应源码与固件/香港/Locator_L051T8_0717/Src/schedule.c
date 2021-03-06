/*******************************************************************************
Copyright:       Lierda WSN BT team
Filename:        schedule.c
Description:     schedule_mode
FileVersion:     V1.0

ChangeLog: 

=========================================
date:18/2/27
author:chenkl
log: Create File

*******************************************************************************/


/*******************************************************************************
* INCLUDES
*/
//#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stm32l0xx.h"
#include "stm32l0xx_hal.h"
#include "stm32l0xx_it.h"
#include "User_ClockDriver.h"
#include "rtc.h"
#include "usart.h"
#include "tools.h"
#include "schedule.h"
#include "main.h"
#include "nb.h"
/*********************************************************************
* TYPEDEFS
*/
/*********************************************************************
* MACROS
*/
/*********************************************************************
* PUBLIC VARIABLES 
*/
RTC_TimeTypeDef stimestructure;
RTC_DateTypeDef sdatestructure;
Clock_Struct TimeSynclock;

//服务器超时重发
uint8_t Time_Syn_Retry_Num;

//该天是否同步过标志位
uint8_t Time_synFlag = RESET;
//开机后是否同步过时间戳
uint8_t Time_synInitFlag = RESET;
//同步次数超
uint8_t Time_Syn_overFlow = RESET;

//是否配置过schedule模式
uint8_t Schedule_ModeInitFlag = RESET;
//当前是否是schedule模式
uint8_t Schedule_ModeFlag = RESET;
//C2命令执行结果
Schedule_CMD_State_t Schedule_CMD_State;

uint8_t Schedule_State = RESET;
uint8_t Schedule_StateStartFlag = RESET;
uint8_t Schedule_StateSend_counter = 0;
//向服务器请求同步时间戳标志位
uint8_t Time_synReq = 0x00;
//第一次配置schedule
uint8_t schedule_congif_Flag = RESET;

/*Schedule配置回复（0xD5)*/
//命令执行结果
uint8_t ScheduleConfigResult; 
//分包序号
uint8_t ScheduleConfigSeqNum; 

//时区
uint8_t zoneTime;

//schedule下的模式和周期
Schedule_Mode_t Schedule_Mode_Data;

//当前是在循环中的第几天
uint8_t CurrentDayInTheLoop = 0;
//正在同步时间戳
uint8_t Time_syning_Flag = RESET;

//开始周期性发送数据标志
uint8_t cycleStartFlag = RESET;

//Calendar_t Calendar;
//schedule模式配置表
Calendar_t CalendarList[CYCLEDAYS_MAX][HOURS_GROUP_MAX];
Calendar_t CalendarList2[CYCLEDAYS_MAX][HOURS_GROUP_MAX];

//临时存储上次的配置表以防配置失败
//Calendar_t CalendarList_Temp[CYCLEDAYS_MAX][HOURS_GROUP_MAX] = {0};
//schedule模式信息
Schedule_CMD_t Schedule_CMD;
//信号强度
Signal_Level_t Signal_Level;
//schdedule 模式数据
Schedule_Data_upload_t Schedule_Data_upload;

uint8_t DaySwitchCounter = 0;
/*********************************************************************
* User Timer API
*/

/*
* @fn       setRTC_Date_Time
* @brief    设定RTC时间，年月日，时分秒，星期
* @param   
* @return   
*/
uint8_t setRTC_Date_Time(uint8_t yy, uint8_t mm, uint8_t dd, uint8_t hours, uint8_t mins, uint8_t sec, uint8_t week)
{
    uint8_t dateCorrectFlag = ERROR;
    //大小月判断
    if( mm==4 || mm==6 )
    {
        if(dd>30)
        {
            return ERROR;
        }
    }
    if( mm==9 || mm==11 )
    {
        if(dd>30)
        {
            return ERROR;
        }
    }
    //闰年的判断 -- 地绕日运行周期365.24219天。四年一闰，百年不闰，四百年再闰
    if( yy%4 != 0 )
    {
        //非闰年大于28号为异常
        if( mm==2 && dd>28 )
        {
            return ERROR;
        }
    }
    else
    {
        //闰年大于29号为异常
        if( mm==2 && dd>29 )
        {
            return ERROR;
        }
    }
    //判断其余格式
    if( mm>0 && mm<=12 )
    {
        if(dd>0 && dd<=31)
        {
            if( hours<24 )
            {
                if( mins<60 )
                {
                    if( sec<60 )
                    {
                        if( week>0 && week<=7 )
                        {
                            dateCorrectFlag = SUCCESS;
                        }
                    }
                }
            }
        }
    }
    if(dateCorrectFlag == SUCCESS)
    {
        stimestructure.Hours = hours;
        stimestructure.Minutes = mins;
        stimestructure.Seconds = sec;
        stimestructure.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
        stimestructure.StoreOperation = RTC_STOREOPERATION_RESET;
        if (HAL_RTC_SetTime(&hrtc, &stimestructure, RTC_FORMAT_BIN) != HAL_OK)
        {
            printf("SetTimeError\r\n");
            return ERROR;
        }
        
        sdatestructure.WeekDay = week;
        sdatestructure.Month = mm;
        sdatestructure.Date = dd;
        sdatestructure.Year = yy;
        
        if (HAL_RTC_SetDate(&hrtc, &sdatestructure, RTC_FORMAT_BIN) != HAL_OK)
        {
            printf("SetDateError\r\n");
            return ERROR;
        }
    }
    else
    {
        return ERROR;
    }
    return SUCCESS;
}

/*
* @fn       getRTC_Date_Time
* @brief    打印RTC时间，年月日，时分秒，星期
* @param   
* @return   
*/
uint8_t getRTC_Date_Time(void)
{
    uint8_t getTimestate = HAL_ERROR;
    
    if(HAL_OK == HAL_RTC_GetTime(&hrtc, &stimestructure, RTC_FORMAT_BIN))
    {
        if(HAL_OK == HAL_RTC_GetDate(&hrtc, &sdatestructure, RTC_FORMAT_BIN))
        {
            getTimestate = HAL_OK;
        }
    }

    /* Display date Format : yy/mm/dd */
    //printf("%02d/%02d/%02d\r\n",2000 + sdatestructure.Year, sdatestructure.Month, sdatestructure.Date); 
    /* Display time Format : hh:mm:ss */
    if(getTimestate == HAL_OK)
    {
        printf("%02d:%02d:%02d\r\n",stimestructure.Hours, stimestructure.Minutes, stimestructure.Seconds);
    }
    else
    {
        printf("getRTC_Date_ERROR\r\n");
    }
    return getTimestate;
    //printf("week:%02d\r\n",sdatestructure.WeekDay);
    //printf("\r\n");
}


///*
//* @fn       Judge_Time_syn_avivilble
//* @brief    判断时间是否有效，并同步更新本地时间
//* @param   
//* @return   
//*/
//void Judge_Time_syn_avivilble(uint8_t yy, uint8_t mm, uint8_t dd, uint8_t hours, uint8_t mins, uint8_t sec, uint8_t week)
//{
//    getRTC_Date_Time();
//    uint8_t t=0xFF;
//#warning 待修改0309    
//    //当前时间不跨天，且时间差小于3s
//    if(dd == sdatestructure.Date)
//    {
//        if(mins == stimestructure.Minutes)
//        {
//            if( sec >= stimestructure.Seconds )
//            {
//                t = sec - stimestructure.Seconds;
//            }
//            else
//            {
//                t = stimestructure.Seconds - sec;
//            }
//            if(t<4)
//            {
//                if(setRTC_Date_Time(yy, mm, dd, hours, mins, sec, week))
//                {
//                    Time_synFlag = SET;
//                    //今天同步已完成
//                    User_StopClock(&TimeSynclock);
//                    Time_Syn_Retry_Num = 0;
//                }
//            }
//        }
//    }
//}


/*
* @fn       switchToToday
* @brief    0点时清空时间同步
            判断并切换到这一天
* @param   
* @return   今天是第几天/周几
*/
switchToToday_t switchToToday(uint8_t schedule_mode)
{
    switchToToday_t switchToToday = {0};
    if( getRTC_Date_Time() == HAL_OK )
    {
        if(stimestructure.Hours == 0)
        {
            if(stimestructure.Minutes == 0)
            {
                //#warning 奇怪的现象！！！            
                //            uint8_t test1;
                //            if(RegistFlag == SET)
                //            {
                //               test1 = 20; 
                //            }
                //            else 
                //            {
                //                test1 = 40;
                //            }
                
                if(stimestructure.Seconds >0 && stimestructure.Seconds<60)
                {
                    if(DaySwitchCounter == 0)
                    {
                        if(RegistFlag == SET)
                        {
                            DaySwitchCounter = 1;
                        }
                        
                        if(gps_open_Flag == SET)
                        {
                            User_StopClock(&GPSTimeOutclock);
                            Device_Close_GPS();
                        }
                        if(NB_open_Flag == SET)
                        {
                            User_StopClock(&ATTimeoutclock);
                            User_StopClock(&InNetTimeoutclock);
                            User_StopClock(&DataRptclock);
                            User_StopClock(&DataRspclock);
                            User_StopClock(&BoardCloseTimeoutclock);
                            NB_open_Flag = RESET;
                            Close_NB();
                        }
                        //切换至明天
                        switchToToday.DaySwitchFlag = SET;
                        switch(schedule_mode)
                        {
                        case SCHEDULE_MODE_DAYCYCLE:
                            {
                                CurrentDayInTheLoop = (CurrentDayInTheLoop + 1)%Schedule_CMD_State.dayrange;
                                switchToToday.CurrentDayInTheLoop = CurrentDayInTheLoop;
                            }
                            break;
                            
                        case SCHEDULE_MODE_WEEKCYCLE:
                            {
                                switchToToday.CurrentDayInTheLoop = sdatestructure.Date;
                            }
                            break;
                        case SCHEDULE_MODE_WEEKCYCLEONETIME:
                            {
                                switchToToday.CurrentDayInTheLoop = sdatestructure.Date;
                            }
                            break;    
                            
                        default:
                            break;
                        }
                    }
                }
            }
        }
        //12点同步时间戳
        if(stimestructure.Hours == 11)
        {
            if(stimestructure.Minutes == 59)
            {
                if(stimestructure.Seconds >=40 && stimestructure.Seconds<59)
                {
                    switchToToday.TimeSynFlag = SET;
                    if(gps_open_Flag == SET)
                    {
                        User_StopClock(&GPSTimeOutclock);
                        Device_Close_GPS();
                    }
                    if(NB_open_Flag == SET)
                    {
                        User_StopClock(&ATTimeoutclock);
                        User_StopClock(&InNetTimeoutclock);
                        User_StopClock(&DataRptclock);
                        User_StopClock(&DataRspclock);
                        User_StopClock(&BoardCloseTimeoutclock);
                        NB_open_Flag = RESET;
                        Close_NB();
                    }
                }
            }
        }
    }
    else
    {
        switchToToday.RTC_ERROR = SET;
    }
    return switchToToday;
}

/*
* @fn       endWeekCycle
* @brief    按周执行一次的结束判断
* @param   
* @return   结束标志位
*/
uint8_t endWeekCycle(void)
{
    getRTC_Date_Time();
    if(sdatestructure.WeekDay == RTC_WEEKDAY_SUNDAY)
    {
        if(stimestructure.Hours == 23)
        {
            if(stimestructure.Minutes == 59)
            {
                if(stimestructure.Seconds >=20 && stimestructure.Seconds<60)
                {
                    return SET;
                }
            }
        }
    }
    return RESET;
}



/*
* @fn       getFollowedMode
* @brief    根据今天是周几
            查询并输出下次上报周期及模式
* @param   
* @return   下次上报的周期和模式
*/
Schedule_Mode_t getFollowedMode(uint8_t week)
{
    uint8_t date;
    uint8_t start_h = 47;      //0~23点半
    uint8_t end_h = 48;        //1~24点
    
    uint8_t start_l;           //列表中编号
    uint8_t end_l;             //列表中编号
    uint8_t i;
    uint8_t RTC_start_hour = 0xFF;    //当前的RTC时间段0~47 如:1 表示大于0:30 小于1:00 的时间段
    uint8_t Schedule_startFlag = RESET;
    uint8_t Schedule_endFlag = RESET;
    
    date = week-1;
    Schedule_Mode_t Schedule_Mode;
    Schedule_Mode.cycle = DEFAULT_S_CYCLE;
    Schedule_Mode.mode =  POSITIONING_MODE;
        
    //getRTC_Date_Time();
    
    //半小时为存储单位
    for(i=0; i<24; i++)
    {
        if(i == stimestructure.Hours)
        {
            if(stimestructure.Minutes>=30)
            {
                RTC_start_hour = i*2+1;
            }
            else
            {
                RTC_start_hour = i*2;
            }
            break;
        }
    }
    
    for(i=0; i<HOURS_GROUP_MAX; i++)
    {
        if(CalendarList[date][i].end_Time!=0)
        {
            //获取最近的开始时间
            if(CalendarList[date][i].start_Time > RTC_start_hour  && CalendarList[date][i].start_Time < start_h)
            {
                Schedule_startFlag = SET;
                start_h = CalendarList[date][i].start_Time;
                start_l = i;
                break;
            }
            
        }
    }
    for(i=0; i<HOURS_GROUP_MAX; i++)
    {
        if(CalendarList[date][i].end_Time!=0)
        {
            //获取最近的结束时间
            if(CalendarList[date][i].end_Time > RTC_start_hour  && CalendarList[date][i].end_Time <= end_h)
            {
                Schedule_endFlag = SET;
                end_h = CalendarList[date][i].end_Time;
                end_l = i;
                break;
            }
        }
    }
    //if(start_h != end_h)
    //{
        uint16_t Tn;     //当前时间
        uint16_t Ts;     //schedule列表中存储的数据中，距现在最近的开始时间
        Tn = stimestructure.Hours*60 + stimestructure.Minutes;
        
        
        if(Schedule_endFlag == SET)
        {
            if(RTC_start_hour == 47 || RTC_start_hour == 46)
            {
                uint16_t cycle;    //该时间段下配置的周期
                cycle = BigtoLittle16(CalendarList[date][end_l].period);
                Ts = CalendarList[date][end_l].start_Time*30;
                
                Schedule_Mode.cycle = ((Tn - Ts)/cycle + 1)*cycle - (Tn - Ts);
                Schedule_Mode.mode = CalendarList[date][end_l].mode;
                Schedule_Mode.config_cycle = CalendarList[date][end_l].period;
            }
            else
            {
                //如果离最近的开始时间近 下次上报时间为 开始时间-现在时间
                if(start_h < end_h && Schedule_startFlag == SET)
                {
                    Ts = start_h*30;
                    //转换为分钟
                    Schedule_Mode.cycle = Ts - Tn;
                    Schedule_Mode.mode = CalendarList[date][start_l].mode;
                    Schedule_Mode.config_cycle = 0;//CalendarList[date][start_l].period;
                }
                else
                {
                    uint16_t cycle;    //该时间段下配置的周期
                    cycle = BigtoLittle16(CalendarList[date][end_l].period);
                    Ts = CalendarList[date][end_l].start_Time*30;
                    
                    Schedule_Mode.cycle = ((Tn - Ts)/cycle + 1)*cycle - (Tn - Ts);
                    Schedule_Mode.mode = CalendarList[date][end_l].mode;
                    Schedule_Mode.config_cycle = CalendarList[date][end_l].period;
                }
            }
        }
        //这一天为非schedule模式 --Tracking 24h
        else
        {
            Schedule_Mode.mode = POSITIONING_MODE;
            Schedule_Mode.cycle = DEFAULT_T_CYCLE;
            Schedule_Mode.config_cycle = 0;//BigtoLittle16(DEFAULT_T_CYCLE);
        }
    //}

#warning 新增混合模式
        uint8_t start_h2 = 47;      //0~23点半
        uint8_t end_h2 = 48;        //1~24点
        
        uint8_t start_l2;           //列表中编号
        uint8_t end_l2;             //列表中编号
        uint8_t Schedule_startFlag2 = RESET;
        uint8_t Schedule_endFlag2 = RESET;
        
        Schedule_Mode_t Schedule_Mode2;
        Schedule_Mode2.cycle = DEFAULT_S_CYCLE;
        Schedule_Mode2.mode =  POSITIONING_MODE;
        
        
        for(i=0; i<HOURS_GROUP_MAX; i++)
        {
            if(CalendarList2[date][i].end_Time!=0)
            {
                //获取最近的开始时间
                if(CalendarList2[date][i].start_Time > RTC_start_hour  && CalendarList2[date][i].start_Time < start_h2)
                {
                    Schedule_startFlag2 = SET;
                    start_h2 = CalendarList2[date][i].start_Time;
                    start_l2 = i;
                    break;
                }
                
            }
        }
        for(i=0; i<HOURS_GROUP_MAX; i++)
        {
            if(CalendarList2[date][i].end_Time!=0)
            {
                //获取最近的结束时间
                if(CalendarList2[date][i].end_Time > RTC_start_hour  && CalendarList2[date][i].end_Time <= end_h2)
                {
                    Schedule_endFlag2 = SET;
                    end_h2 = CalendarList2[date][i].end_Time;
                    end_l2 = i;
                    break;
                }
            }
        }
        uint16_t Tn2;     //当前时间
        uint16_t Ts2;     //schedule列表中存储的数据中，距现在最近的开始时间
        Tn2 = stimestructure.Hours*60 + stimestructure.Minutes;
        
        if(Schedule_endFlag2 == SET)
        {
            if(RTC_start_hour == 47 || RTC_start_hour == 46)
            {
                uint16_t cycle;    //该时间段下配置的周期
                cycle = BigtoLittle16(CalendarList2[date][end_l2].period);
                Ts2 = CalendarList2[date][end_l2].start_Time*30;
                
                Schedule_Mode2.cycle = ((Tn2 - Ts2)/cycle + 1)*cycle - (Tn2 - Ts2);
                Schedule_Mode2.mode = CalendarList2[date][end_l2].mode;
                Schedule_Mode2.config_cycle = CalendarList2[date][end_l2].period;
            }
            else
            {
                //如果离最近的开始时间近 下次上报时间为 开始时间-现在时间
                if(start_h2 < end_h2 && Schedule_startFlag2 == SET)
                {
                    Ts2 = start_h2*30;
                    //转换为分钟
                    Schedule_Mode2.cycle = Ts2 - Tn2;
                    Schedule_Mode2.mode = CalendarList2[date][start_l2].mode;
                    Schedule_Mode2.config_cycle = 0;//CalendarList2[date][start_l2].period;
                }
                else
                {
                    uint16_t cycle;    //该时间段下配置的周期
                    cycle = BigtoLittle16(CalendarList2[date][end_l2].period);
                    Ts2 = CalendarList2[date][end_l2].start_Time*30;
                    
                    Schedule_Mode2.cycle = ((Tn2 - Ts2)/cycle + 1)*cycle - (Tn2 - Ts2);
                    Schedule_Mode2.mode = CalendarList2[date][end_l2].mode;
                    Schedule_Mode2.config_cycle = CalendarList2[date][end_l2].period;
                }
            }
        }
        //这一天为非schedule模式 --Tracking 24h
        else
        {
            Schedule_Mode2.mode = POSITIONING_MODE;
            Schedule_Mode2.cycle = DEFAULT_T_CYCLE;
            Schedule_Mode2.config_cycle = 0;//BigtoLittle16(DEFAULT_T_CYCLE);
            
        }
        
        if(Schedule_Mode2.cycle <= Schedule_Mode.cycle)
        {
            Schedule_Mode.cycle = Schedule_Mode2.cycle;
            Schedule_Mode.mode = Schedule_Mode2.mode;
            Schedule_Mode.config_cycle = Schedule_Mode2.config_cycle;
        }
    return Schedule_Mode;
}
//schedule数据上报0xD3
/*
* @fn      Schedule_Data_Report()
* @brief   向服务器上报schedule数据
* @param   
* @return   
*/
void Schedule_Data_Report(NB_Report_to_ServerData_t NB_Report_to_ServerData_tmp, Schedule_Data_upload_t Schedule_Data_upload, GPS_Info_t Schedule_GPS_Info)
{
    uint16_t report_to_ServerDataLen_D;
    uint8_t ServerDataLenSize;
    
    //协议数据填充长度
    uint8_t NB_Report_to_ServerData_Len;
    uint8_t Schedule_Data_upload_Len;
    uint8_t Schedule_GPS_Data_Len;
    
    uint16_t SumLen;
    //        NB_Report_to_ServerData_tmp.cmdlen = BigtoLittle16(NB_Report_to_ServerData_tmp.cmdlen);
    //        Locator_Data_tmp.FindnowCycle = BigtoLittle16(Locator_Data_tmp.FindnowCycle);
    //        Locator_Data_tmp.TrackerCycle = BigtoLittle16(Locator_Data_tmp.TrackerCycle);
    //        Locator_Data_tmp.GPS_data_len = BigtoLittle16(Locator_Data_tmp.GPS_data_len);
    
    
    Schedule_Data_upload.cycle = Schedule_Data_upload.cycle; //BigtoLittle16(Schedule_Data_upload.cycle);
    Schedule_Data_upload.cellID = Signal_Level.cellID;
    Schedule_Data_upload.ECL = Signal_Level.ECL;
    Schedule_Data_upload.RSPR = Signal_Level.RSPR;
    Schedule_Data_upload.RSRQ = Signal_Level.RSRQ;
    Schedule_Data_upload.SINR = Signal_Level.SINR;
    
    NB_Report_to_ServerData_tmp.cmdlen = BigtoLittle16(NB_Report_to_ServerData_tmp.cmdlen);
    
    //字段长度
    NB_Report_to_ServerData_Len = sizeof(NB_Report_to_ServerData_t);
    Schedule_Data_upload_Len = sizeof(Schedule_Data_upload_t);
    Schedule_GPS_Data_Len = sizeof(GPS_List_t);
    
    //总数据长度
    report_to_ServerDataLen_D = NB_Report_to_ServerData_Len + Schedule_Data_upload_Len + Schedule_Data_upload.GPS_data_len + 2;
    
    //大小端转换
    Schedule_Data_upload.GPS_data_len = BigtoLittle16(Schedule_Data_upload.GPS_data_len);
    
    //组成AT指令帧头
    uint16_t dataLen_t;
    uint8_t datadis;
    if(Schedule_Data_upload.GPS_List_num == 0)
    {
        dataLen_t = report_to_ServerDataLen_D - 2;
        datadis = 4;
    }
    else if(Schedule_Data_upload.GPS_List_num > 0)
    {
        dataLen_t = report_to_ServerDataLen_D - 1;
        datadis = 2;
    }
    ServerDataLenSize = Form_AT_Head(dataLen_t);
    SumLen = AT_TEST_PORT_LEN + ServerDataLenSize + (report_to_ServerDataLen_D + CHECK_SUM_LEN)*2 + AT_TAIL_NUM;
    
    //添加NB字段与定位器字段数据
    uint16_t off_set_Len = 0;
    uint8_t at_head_len = 0;
    //NB包头
    off_set_Len = AT_TEST_PORT_LEN + ServerDataLenSize;
    at_head_len = AT_TEST_PORT_LEN + ServerDataLenSize;
    
    hex2String((uint8_t*)&NB_Report_to_ServerData_tmp, NB_Report_to_ServerData_Len, (char*)at_cmd_data_p + at_head_len);
    
    off_set_Len = off_set_Len + NB_Report_to_ServerData_Len*2;
    //定位器包头
    hex2String((uint8_t*)&Schedule_Data_upload, Schedule_Data_upload_Len, (char*)at_cmd_data_p + off_set_Len);
    
    off_set_Len = off_set_Len + Schedule_Data_upload_Len*2;
    
    //添加校验和
    uint8_t crc = 0;
    
    //GPS数据
    for(uint8_t i=0; i<Schedule_Data_upload.GPS_List_num; i++)
    {
        hex2String((uint8_t*)&Schedule_GPS_Info.GPS_List[i], Schedule_GPS_Data_Len, (char*)at_cmd_data_p + off_set_Len);           
        off_set_Len = off_set_Len + Schedule_GPS_Data_Len*2;
        crc = crc + checkSum((uint8_t *)&Schedule_GPS_Info.GPS_List[i], Schedule_GPS_Data_Len);
    }
    
    //计算除去包头之外的数据
    crc = crc + checkSum((uint8_t *)&NB_Report_to_ServerData_tmp+1, NB_Report_to_ServerData_Len-1); //除去0xFA
    crc = crc + checkSum((uint8_t *)&Schedule_Data_upload, Schedule_Data_upload_Len);
    
    hex2String( &crc, CHECK_SUM_LEN, (char*)at_cmd_data_p + off_set_Len);
    
    
    //添加结束位
    uint8_t server_end;
    server_end = SERVER_START;
    hex2String(&server_end, 1, (char*)at_cmd_data_p + off_set_Len + CHECK_SUM_LEN*2);
    off_set_Len = off_set_Len + 2;
    
    //添加"\r\n"
    tool_memcpy((char*)at_cmd_data_p + off_set_Len + CHECK_SUM_LEN*2, AT_END, AT_TAIL_NUM);
    
    HAL_UART_Transmit_DMA(&hlpuart1, (uint8_t *)at_cmd_data_p, SumLen-datadis);
    printf("S_UP\r\n");
    //HAL_UART_Transmit_DMA(&huart2, (uint8_t *)at_cmd_data_p, SumLen-datadis);
}

//schedule数据上报0xD4,D5
/*
* @fn      Schedule_Device_Confirm_Report
* @brief   向服务器上报终端确认命令
* @param   
* @return   
*/
void Schedule_Device_Confirm_Report(uint8_t CMD_ID, uint8_t serviceCMD_Process_result, uint8_t packetSeq, uint8_t zoneTime, uint8_t TimeSynFlag)
{
    uint8_t report_to_ServerDataLen_D = 0;
    uint16_t SumLen = 0;
    uint8_t ServerDataLenSize;
    uint8_t crc;
    
    //添加终端确认命令 字段数据
    NB_Report_to_ServerData_t Device_confirm_Data = {0};
    Device_confirm_Data.start = SERVER_START;
    Device_confirm_Data.version = 0x01;
    Device_confirm_Data.softWare_version = 0x02;
    Device_confirm_Data.msg_seq_num = BigtoLittle16(msg_seq_num);
    
    //Device_confirm_Data.msg_seq_num = BigtoLittle16(msg_seq_num - 1);
    tool_memcpy(Device_confirm_Data.IMEI_num, NB_Report_to_ServerData.IMEI_num, IMEI_NUM_LEN); 
    
    if(CMD_ID == SCHEDULE_CONFIG_ID)
    {
        Device_confirm_Data.cmd = SCHEDULE_CONFIG_ID;
        report_to_ServerDataLen_D = sizeof(NB_Report_to_ServerData_t) + 3 + 1;
    }
    //0xD5
    else if(CMD_ID == TIME_SYN_REQ_ID)
    {
        Device_confirm_Data.cmd = TIME_SYN_REQ_ID;
        report_to_ServerDataLen_D = sizeof(NB_Report_to_ServerData_t) + 3 + 1;
    }
    
    Device_confirm_Data.cmdlen = BigtoLittle16(report_to_ServerDataLen_D);
    //传递数据总长
    ServerDataLenSize = Form_AT_Head(report_to_ServerDataLen_D-1);
    SumLen = AT_TEST_PORT_LEN + (report_to_ServerDataLen_D + CHECK_SUM_LEN)*2 + AT_TAIL_NUM;
    
    //添加命令执行结果字段数据
    uint8_t off_set_Len = 0;  
    off_set_Len = AT_TEST_PORT_LEN + ServerDataLenSize;
    
    crc = checkSum((uint8_t*)&Device_confirm_Data, sizeof(NB_Report_to_ServerData_t));
    crc = crc - SERVER_START;
    
    
    hex2String((uint8_t*)&Device_confirm_Data, sizeof(NB_Report_to_ServerData_t), (char*)at_cmd_data_p+off_set_Len);
    off_set_Len = off_set_Len + sizeof(NB_Report_to_ServerData_t)*2;
    //0xD5
    if(CMD_ID == TIME_SYN_REQ_ID)
    {
        crc = crc + zoneTime;
        //命令结果
        hex2String(&zoneTime, 1, (char*)at_cmd_data_p + off_set_Len);
        off_set_Len = off_set_Len + 2;
        
        //命令结果
        hex2String(&TimeSynFlag, 1, (char*)at_cmd_data_p + off_set_Len);
        off_set_Len = off_set_Len + 2;
    }
    else if(CMD_ID == SCHEDULE_CONFIG_ID)
    {
        D4Flag = SET;
        
        crc = crc + serviceCMD_Process_result + packetSeq;
        //分包序号
        hex2String(&packetSeq, 1, (char*)at_cmd_data_p + off_set_Len);
        off_set_Len = off_set_Len + 2;
        //命令结果
        hex2String(&serviceCMD_Process_result, 1, (char*)at_cmd_data_p + off_set_Len);
        off_set_Len = off_set_Len + 2;
    }
    
    //添加校验和
    hex2String(&crc, 1, (char*)at_cmd_data_p + off_set_Len);
    off_set_Len = off_set_Len + CHECK_SUM_LEN*2;
    
    //添加结束位
    uint8_t server_end;
    server_end = SERVER_START;
    hex2String(&server_end, 1, (char*)at_cmd_data_p + off_set_Len);
    off_set_Len = off_set_Len + 2;
    //添加"\r\n"
    tool_memcpy(at_cmd_data_p + off_set_Len, AT_END, AT_TAIL_NUM);
    
    HAL_UART_Transmit_DMA(&hlpuart1, (uint8_t *)at_cmd_data_p, SumLen);
    //printf("D4,D5\r\n");
    //HAL_UART_Transmit_DMA(&huart2, (uint8_t *)at_cmd_data_p, SumLen);
}



//schedule数据下发0xC2
/*
* @fn      Handel_ServerData_Schedule_Config
* @brief   处理服务器的 Schedule模式配置
* @param   
* @return  命令是否执行 完成/成功 
*/
Schedule_CMD_State_t Handel_ServerData_Schedule_Config(uint8_t* RxMessage, uint16_t dataLen)
{
    //当次循环安排
    Schedule_Data_t Schedule_Data;
    //当天安排
    Schedule_DayConfig_t Schedule_DayConfig;
    //命令执行结果
    Schedule_CMD_State_t Schedule_CMD_State;
    
    uint16_t off_set_Len = 0;
    uint8_t format_judge = ERROR;
    //多包数据是否已经接收完成
    uint8_t packageFinishFlag = RESET;
    
    ascsToHexs((char*)RxMessage, (uint8_t*)&Schedule_CMD, sizeof(Schedule_CMD_t)*2);
    off_set_Len = off_set_Len + sizeof(Schedule_CMD_t)*2;
    
    Schedule_CMD.scheduleDataLen = BigtoLittle16(Schedule_CMD.scheduleDataLen);
    //包序号
    Schedule_CMD_State.packetNum = CMD_packageNum+1;
    //schedule下的模式
    Schedule_CMD_State.dayrange = Schedule_CMD.scheduleDays;
    Schedule_CMD_State.configMode = Schedule_CMD.ModeFlag;
    //包结尾标志位
    if(Schedule_CMD.seqNum == 0)
    {
        packageFinishFlag = SET;
    }
    
    for(uint8_t i=0; i<Schedule_CMD.scheduleDataGroupNum; i++)
    {
        ascsToHexs((char*)RxMessage+off_set_Len, (uint8_t*)&Schedule_Data, sizeof(Schedule_Data_t)*2);
        off_set_Len = off_set_Len + sizeof(Schedule_Data_t)*2;
        for(uint8_t j=0; j<Schedule_Data.calendar_num; j++)
        {
            ascsToHexs((char*)RxMessage+off_set_Len, (uint8_t*)&Schedule_DayConfig, sizeof(Schedule_DayConfig_t)*2);
            off_set_Len = off_set_Len + sizeof(Schedule_DayConfig_t)*2;
            //格式判断
            format_judge = ERROR;
            if(Schedule_DayConfig.startH < Schedule_DayConfig.endH)
            {
                if(Schedule_DayConfig.startH<=47)
                {
                     if(Schedule_DayConfig.endH>0 && Schedule_DayConfig.endH<=48)
                     {
                         if(Schedule_DayConfig.mode <= POSITIONING_MODE)
                         {
                             //if((Schedule_DayConfig.endH - Schedule_DayConfig.startH)*30 >= Schedule_DayConfig.cycle)
                             //{
                                format_judge = SUCCESS;
                             //}
                         }
                     }
                }
            }
            if(format_judge == ERROR)
            {
                Schedule_CMD_State.cmdResult = ERROR;
                return Schedule_CMD_State;
            }
            //保存当天安排
#warning 新增混合模式522
            printf("hybridFlag:%d\r\n", Schedule_CMD.hybridFlag);
            if(Schedule_CMD.hybridFlag == RESET)
            {
                printf("111\r\n");
                tool_memcpy(&CalendarList[Schedule_Data.schedulePeriod-1][j], &Schedule_DayConfig, sizeof(Schedule_DayConfig_t));
            }
            else
            {
                printf("222\r\n");
                tool_memcpy(&CalendarList2[Schedule_Data.schedulePeriod-1][j], &Schedule_DayConfig, sizeof(Schedule_DayConfig_t));
            }
        }
    }
    if(packageFinishFlag == SET)
    {
        Schedule_CMD_State.cmdResult = SUCCESS;
        //保存该scheduleID等
        Schedule_Data_upload.scheduleID = Schedule_CMD.scheduleID;

        return Schedule_CMD_State;
    }
    else
    {
        Schedule_CMD_State.cmdResult = PACKAGE_NO_FINISH;
        return Schedule_CMD_State;
    }
}
//schedule数据下发0xC4 -- 同步时间戳
/*
* @fn      Handel_ServerData_TimeSyn
* @brief   根据服务器下发数据更新时间戳
* @param   
* @return   
*/
uint8_t Handel_ServerData_TimeSyn(uint8_t* RxMessage, uint16_t dataLen)
{
    Time_syn_Data_t Time_syn_Data;
    //年月日，时分秒
    uint8_t yy, mm, dd, hour, min, sec, week; 
    
    ascsToHexs((char*)RxMessage, (uint8_t*)&Time_syn_Data, sizeof(Time_syn_Data_t)*2);
    
    yy = hex_to_dig(Time_syn_Data.Date_Value[0]);
    mm = hex_to_dig(Time_syn_Data.Date_Value[1]);
    dd = hex_to_dig(Time_syn_Data.Date_Value[2]);
    
    hour = hex_to_dig(Time_syn_Data.Local_Time[0]);
    min = hex_to_dig(Time_syn_Data.Local_Time[1]);
    sec = hex_to_dig(Time_syn_Data.Local_Time[2]);
    
    week = Time_syn_Data.week;
        
    if(SUCCESS != setRTC_Date_Time(yy, mm, dd, hour, min, sec, week))
    //if(SUCCESS != setRTC_Date_Time(18, 5, 3, 11, 58, 40, 4))
    {
        return ERROR;
    }
    //Time_synFlag = SET;
    printf("Time_syn_SET\r\n");

    return SUCCESS;
}

/*--------------------------------------测试用-------------------------------------*/
/*
* @fn      TLocator_Data_Report()
* @brief   向服务器上报定位器数据
* @param   
* @return   
*/
void TLocator_Data_Report(NB_Report_to_ServerData_t NB_Report_to_ServerData_tmp, Locator_Data_t Locator_Data_tmp, GPS_Info_t GPS_Info_tmp, Signal_Level_t Signal_Level)
{
        uint16_t report_to_ServerDataLen_D;
        uint8_t ServerDataLenSize;
        uint8_t NB_Report_to_ServerData_Len;
        uint8_t Locator_Data_Len;
        uint8_t GPS_Data_Len;
        uint8_t Signal_Level_Len;
        uint16_t SumLen;
        
        NB_Report_to_ServerData_tmp.cmdlen = BigtoLittle16(NB_Report_to_ServerData_tmp.cmdlen);
        Locator_Data_tmp.FindnowCycle = BigtoLittle16(Locator_Data_tmp.FindnowCycle);
        Locator_Data_tmp.TrackerCycle = BigtoLittle16(Locator_Data_tmp.TrackerCycle);
        Locator_Data_tmp.GPS_data_len = BigtoLittle16(Locator_Data_tmp.GPS_data_len);    
               
        //字段长度
        NB_Report_to_ServerData_Len = sizeof(NB_Report_to_ServerData_t);
        Locator_Data_Len = sizeof(Locator_Data_t);
        GPS_Data_Len = sizeof(GPS_List_t);
        Signal_Level_Len = sizeof(Signal_Level_t);
        
        //总数据长度
        report_to_ServerDataLen_D = NB_Report_to_ServerData_Len + Locator_Data_Len + (Locator_Data_tmp.GPS_List_num)*GPS_Data_Len + Signal_Level_Len + 1;
        
        
        //组成AT指令帧头
        ServerDataLenSize = Form_AT_Head(report_to_ServerDataLen_D);
        SumLen = AT_TEST_PORT_LEN + ServerDataLenSize + (report_to_ServerDataLen_D + CHECK_SUM_LEN)*2 + AT_TAIL_NUM;
          
        //添加NB字段与定位器字段数据
        uint16_t off_set_Len = 0;
        uint8_t at_head_len = 0;
        //NB包头
        off_set_Len = AT_TEST_PORT_LEN + ServerDataLenSize;
        at_head_len = AT_TEST_PORT_LEN + ServerDataLenSize;
        
        hex2String((uint8_t*)&NB_Report_to_ServerData_tmp, NB_Report_to_ServerData_Len, (char*)at_cmd_data_p + at_head_len);
        
        off_set_Len = off_set_Len + NB_Report_to_ServerData_Len*2;
        
        //信号强度包头
        hex2String((uint8_t*)&Signal_Level, Signal_Level_Len, (char*)at_cmd_data_p + off_set_Len);
        off_set_Len = off_set_Len + Signal_Level_Len*2;
        
        //定位器包头
        hex2String((uint8_t*)&Locator_Data_tmp, Locator_Data_Len, (char*)at_cmd_data_p + off_set_Len);
        off_set_Len = off_set_Len + Locator_Data_Len*2;
               
        //添加校验和
        uint8_t crc = 0;
        
        //GPS数据
        for(uint8_t i=0; i<Locator_Data_tmp.GPS_List_num; i++)
        {
            hex2String((uint8_t*)&GPS_Info_tmp.GPS_List[i], GPS_Data_Len, (char*)at_cmd_data_p + off_set_Len);           
            off_set_Len = off_set_Len + GPS_Data_Len*2;
            crc = crc + checkSum((uint8_t *)&GPS_Info_tmp.GPS_List[i], GPS_Data_Len);
        }

        //计算除去包头之外的数据
        crc = crc + checkSum((uint8_t *)&NB_Report_to_ServerData_tmp+1, NB_Report_to_ServerData_Len-1); 
        crc = crc + checkSum((uint8_t *)&Locator_Data_tmp, Locator_Data_Len);
        crc = crc + checkSum((uint8_t *)&Signal_Level, Signal_Level_Len);
        
        hex2String( &crc, CHECK_SUM_LEN, (char*)at_cmd_data_p + off_set_Len);        
        
        
        //添加结束位
        uint8_t server_end;
        server_end = SERVER_START;
        hex2String(&server_end, 1, (char*)at_cmd_data_p + off_set_Len + CHECK_SUM_LEN*2);
        off_set_Len = off_set_Len + 2;
        
        //添加"\r\n"
        tool_memcpy((char*)at_cmd_data_p + off_set_Len + CHECK_SUM_LEN*2, AT_END, AT_TAIL_NUM);
        
        HAL_UART_Transmit_DMA(&hlpuart1, (uint8_t *)at_cmd_data_p, SumLen);
        //HAL_UART_Transmit_DMA(&huart2, (uint8_t *)at_cmd_data_p, SumLen);
}

/*
* @fn       Time_synchronization
* @brief    时间同步函数
* @param   
* @return   
*/
void Time_synchronization(void)
{
    //开定时器以及发送计数
    //Time_Syn_overFlow = RESET;
    Time_Syn_Retry_Num = 0;
    User_StartClock(&TimeSynclock);
    
    Schedule_Device_Confirm_Report(TIME_SYN_REQ_ID, 0, Schedule_CMD_State.packetNum, 0xFF, 0);
}

/*
* @fn       Time_syn_Cb
* @brief    时间同步回调
* @param    
* @return   
*/
void Time_syn_Cb(void)
{
    //超时重发
    if(Time_Syn_Retry_Num < TIME_SYN_RETRY_NUM_MAX-1)
    {
        Time_Syn_Retry_Num++;
        
        if(Time_synInitFlag == RESET)
        {
            Schedule_Device_Confirm_Report(TIME_SYN_REQ_ID, 0, Schedule_CMD_State.packetNum, 0xFF, 0);
            curAT_RSP_ID = AT_DATA_RSP;
            User_StartClock(&TimeSynclock);
        }
    }
    //重发次数到切换回原模式，发送
    else
    {
        printf("TB_TimeOut\r\n");
        //Time_Syn_overFlow = SET;
        Time_Syn_Retry_Num = 0;
        
        //切换回原模式
        if(RegistFlag == SET)
        {
            wakeupCounter = 0;
            User_StopClock(&ATTimeoutclock);
            User_StopClock(&InNetTimeoutclock);
            User_StopClock(&DataRptclock);
            User_StopClock(&DataRspclock);
            User_StopClock(&BoardCloseTimeoutclock); 
            SleepStatus = SLEEP;
            printf("SleepA\r\n");
        }
        else
        {
            NVIC_SystemReset();
        }
                
    }
}

/*
* @fn       Time_syn_RetryTimerInit
* @brief    时间同步重发定时
* @param   
* @return   
*/
void Time_syn_RetryTimerInit(void)
{
    TimeSynclock.eventID = TIME_SYN_EVENT;
    TimeSynclock.timeOut = TIME_SYN_TIMEOUT;
    TimeSynclock.TaskHook = Time_syn_Cb;
}




