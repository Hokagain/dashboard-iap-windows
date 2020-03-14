#include <cstdio>
#include <exception>
#include <windows.h>
#include <signal.h>
#include <thread>
#include <mutex>
#include <locale>
#include <iostream>

#include "usb.hpp"

#define uchar unsigned char
#define uint unsigned int
const char* path = "./POTN.bin";


void init();
void cleanAndExit(int sig);
bool informUpgrepStart(USB::Connector& usb);
bool upgrepInform(USB::Connector& usb, USB::Filer& file);
bool upgrepData(USB::Connector& usb, USB::Filer& file);
bool upgrepStatus(USB::Connector& usb);
void controlLoop(USB::Connector& usb, USB::Filer& file);

USB::Filer file;
int main(void)
{  
    try
    {
        init();
        USB::Connector usb;
        controlLoop(usb, file); 
    }
    catch (const std::exception & e)     //�����쳣
    {
        puts(e.what());
    }

    cleanAndExit(0);
}

void init()
{
    signal(SIGABRT, cleanAndExit);  // �쳣��ֹ abort
    signal(SIGTERM, cleanAndExit);  //��ֹ
    signal(SIGINT, cleanAndExit);   //CTRL+C Interrupt from keyboard
    signal(SIGSEGV, cleanAndExit);  //������Ч�ڴ� ��û��Ȩ�޵��ڴ� 
    signal(SIGILL, cleanAndExit);   //ָ�����зǷ�ָ��
    signal(SIGFPE, cleanAndExit);   // �����������

    setlocale(LC_ALL, "chs");      //�������ã�����ANSI ���Ĳ�������

}

void cleanAndExit(int sig)
{
    system("pause");       // DOS���� �ڿ�򲻻����� 
    exit(sig);
}


void controlLoop(USB::Connector& usb,USB::Filer& file) 
{
    while (true) 
    {
        if (!usb.Connect())
        {
            puts("Waiting for device...");
            Sleep(1000);
            continue;
        }
        puts("Device connected!");

        if(!informUpgrepStart(usb))
        {
            puts("Device be missing...");
            Sleep(1000);
            continue;
        }
        puts("Receive STM32 ready upgrep pack");

        if (!upgrepInform(usb,file))
        {
            puts("Send inform failed...");
            Sleep(1000);
            continue;
        }
        puts("Send information pack");

        if (!upgrepData(usb,file))
        {
            puts("send data failed...");
            Sleep(1000);
            continue;
        }
        puts("Send all data pack");

        if(!upgrepStatus(usb))
        {
            puts("checksum is error");
            Sleep(1000);
            continue;
        }
        puts("checksum is OK");
        return ;
    }

}

bool informUpgrepStart(USB::Connector& usb)
{
    BYTE Receive[USB::Connector::MAX_PACK_SIZE];
    BYTE Send[USB::Connector::MAX_PACK_SIZE];
    memset(Send,0,sizeof(Send));

    Send[0] = USB::Connector::UPGRED_PACK;
    while(true)
    {
        Sleep(1000);       
        int actualLen = 0;
        memset(Receive, 0, sizeof(Receive));
        USB::Error err = usb.Read(Receive,sizeof(Receive),&actualLen);
        if (USB::Error::DISCONNECTED == err)
        {
            return false;
        }
        if(USB::Error::OK == err)
        {
            if (usb.UPGRED_READY_PACK == Receive[0])
            {
                return true;
            }
        }

        err = usb.Write(Send, sizeof(Send));
        if (USB::Error::DISCONNECTED == err)
        {
            return false;
        }

    }
}

bool upgrepInform(USB::Connector& usb, USB::Filer& file)
{
    BYTE Receive[USB::Connector::MAX_PACK_SIZE];
    BYTE Send[USB::Connector::MAX_PACK_SIZE];
    memset(Send,0,sizeof(Send));

    Send[0] = usb.UPGRED_INFORM_PACK;
    if (true == file.getBin(path)) {
        Send[1] = (INT8)(file.Size >> 8*3);
        Send[2] = (INT8)(file.Size << 8 >> 8 * 3);
        Send[3] = (INT8)(file.Size << 8*2 >> 8 * 3);
        Send[4] = (INT8)(file.Size << 8 * 3 >> 8 * 3);
        Send[5] = file.Checksum;
        while(true)
        {
            Sleep(1000);
            memset(Receive, 0, sizeof(Receive));
            USB::Error err = usb.Write(Send,sizeof(Send));
            if (USB::Error::DISCONNECTED == err)
            {
                return false;
            }
            if (USB::Error::OK != err)
            {
                continue;
            }

            int actualLen = 0;
            err = usb.Read(Receive,sizeof(Receive),&actualLen);
            if (USB::Error::DISCONNECTED == err)
            {
                return false;
            }
            if (USB::Error::OK != err)
            {
                continue;
            }
            if (usb.ACK_PACK == Receive[0])
            {
                return true;
            }       
        }
    }

}


bool upgrepData(USB::Connector& usb, USB::Filer& file)
{
    USB::Error err;
    BYTE Receive[USB::Connector::MAX_PACK_SIZE];
    BYTE Send[USB::Connector::MAX_PACK_SIZE];
    memset(Send, 0, sizeof(Send));
    int actualLen = 0;
    Send[0] = usb.UPGRED_DATA_PACK;

    while (file.Index/(usb.MAX_PACK_SIZE - 3) < file.Size/(usb.MAX_PACK_SIZE - 3))  //����61�ֽ�ȫ����մ���İ� 
    {
        Send[1] = (INT8)(file.PackNum >> 8);
        Send[2] = (INT8)(file.PackNum << 8 >> 8);
        memset(Receive, 0, sizeof(Receive));
        memcpy(Send + 3,file.Buffer + file.Index,sizeof(Send) - 3);
        err = usb.Write(Send, sizeof(Send));
        if (USB::Error::DISCONNECTED == err)
        {
            return false;
        }

        err = usb.Read(Receive, sizeof(Receive), &actualLen);
        if (USB::Error::DISCONNECTED == err)
        {
            return false;
        }
        if (USB::Error::TIMEOUT == err)     
        {
            continue;
        }
        
        if (usb.ACK_PACK == Receive[0])        //���ͳɹ������ı��+1��buffer����+61
        {
        printf("Send num %d is ok\n", file.PackNum);
        file.PackNum++;
        file.Index += (sizeof(Send) - 3);
        }
    }

    if(file.Size%(usb.MAX_PACK_SIZE - 3) == 0)      //������ֽ����� 61 �������� ���ͽ���
    {
        return true;
    }

    while (true)                          //�������µ� 61�������ֽڵ�����
    {
        Send[1] = (INT8)(file.PackNum >> 8);
        Send[2] = (INT8)(file.PackNum << 8 >> 8);
        memset(Receive, 0, sizeof(Receive));
        memcpy(Send + 3, file.Buffer + file.Index, (file.Size % (usb.MAX_PACK_SIZE - 3)));
        err = usb.Write(Send, sizeof(Send));
        if (USB::Error::DISCONNECTED == err)
        {
            return false;
        }
        err = usb.Read(Receive, sizeof(Receive), &actualLen);
        if (USB::Error::DISCONNECTED == err)
        {
            return false;
        }
        if (USB::Error::TIMEOUT == err)
        {
            continue;
        }
        if (usb.ACK_PACK == Receive[0])          //���ͳɹ������ı��+1��buffer����+61
        {
            printf("Send num %d is ok\n", file.PackNum);
            return true;
        }

    }

}


bool upgrepStatus(USB::Connector& usb)
{
    BYTE Receive[USB::Connector::MAX_PACK_SIZE];
    USB::Error err;
    memset(Receive,0,sizeof(Receive));
    int actualLen = 0;
    while (true)
    {
        err = usb.Read(Receive, sizeof(Receive), &actualLen);
        if (USB::Error::DISCONNECTED == err)
        {
            return false;
        }
        if (USB::Error::TIMEOUT == err)
        {
            continue;
        }
        if (usb.UPGRED_STATUS_PACK == Receive[0])
        {
            if (0 == Receive[1])        // 0 Ϊ�����ɹ�
            {
                return true;
            } 
            return false;     
            
        }

    }

}


// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started:
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file

