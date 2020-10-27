#define     ENT_SIZE    32

#include <stdio.h>
#include <string.h>

typedef unsigned char b; //1 byte
typedef unsigned short w; //word, 2 byte
typedef unsigned int dw; //double word, 4 byte

struct FileNode{
    char name[20];//文件或目录的名称
    char fullname[200];
    //文件或目录的父节点，这种实现方式的不好就是，文件全名长度顶天了200，但是可以设置的更大一些
    dw size; //文件的大小，对目录来说，该值为0
    int start_pos;//内容的起始位置，方便文件读取。.和..为0
    int type; //0为目录，1为文件，-1为分割节点
    int sub_pos; //子目录的起始下标，文件或.或..都为-1
    w cluster; // 文件开始的簇号
};

struct FileNode files[1000]; //最多存储1000个文件节点
int curFiles = 0; //文件列表当前文件个数

#pragma pack(1) //字节对齐最大按照1字节
struct Fat12Header{
    w BPB_BytsPerSec; // 每扇区字节数
    b BPB_SecPerClus;  // 每簇占用的扇区数
    w BPB_RsvdSecCnt; // Boot占用的扇区数
    b BPB_NumFATs;     // FAT表数
    w BPB_RootEntCnt; // 最大根目录文件数
    w BPB_TotSec16;   // 每个FAT占用扇区数
    b BPB_Media;       // 媒体描述符
    w BPB_FATSz16;    // 每个FAT占用扇区数
    w BPB_SecPerTrk;  // 每个磁道扇区数
    w BPB_NumHeads;   // 磁头数
    dw BPB_HiddSec;      // 隐藏扇区数
    dw BPB_TotSec32;     // 如果BPB_TotSec16是0，则在这里记录
};
#pragma pack() // 恢复缺省字节对齐值

struct Fat12Header header;
int Rsvd_Sec, Byts_Per_Sec, Root_Ent_Cnt, Root_Sec, NumFATs, FATSz, Pre_Sec;

/**根目录区条目，用来读取根目录数据
 * 32位
 */
struct RootEntry {
    char name[11];//8.3 file name
    b attr; //文件属性
    char reserves[10]; //保留位
    w wrtTime;
    w wrtDate;
    w fstCluster;
    dw fileSize;
};

/**************************************************************************************************************************************************
 * 工具函数区*/
/** 
 * my_print
 * cnt: the content to print
 * bytes: the length of the content
*/
void my_print(char * cnt, int bytes);
/* change the terminal test to red */
void change_to_red();
/* remove the color settings, back to default */
void back_to_default();
/* 输出换行符 */
void printLF(){
    char * lf = "\n";
    my_print(lf, 1);
}
/* 输出整形数 */
void printNum(w num){
    char numbers[20];
    int size = 1;
    for (w t = num;t>9;t/=10){
        size++;
    }
    for (int i = size-1;i>=0;i--){
    // 不断除10，从低到高
        numbers[i] = num%10 + '0';
        num /= 10;
    }
    my_print(numbers, size);
}
/* 输出报错信息 */
void printError(char * warn, int warnlen, char * cnt, int cntlen){
    change_to_red(); // 设置成醒目的红色
    my_print(warn, warnlen); // 输出错误提示信息
    my_print(cnt, cntlen); // 指出错误内容
    printLF();
    back_to_default(); // 恢复默认颜色
}
/**************************************************************************************************************************************************
 * 文件指令区*/
/* 插入分割节点 */
void splitFiles(){
    //分割节点方便确定一层目录的终止
    files[curFiles++].type = -1;
}
void iniFiles(){
// 前置条件：curFiles == 0
    if (curFiles) return;
    files[curFiles].name[0] = '/';
    files[curFiles].name[1] = 0x00;
    files[curFiles].fullname[0] = '/';
    files[curFiles].fullname[1] = 0x00;
    curFiles++;
    splitFiles();//根目录后需要分割符
}
/**读取引导扇区文件内容，填充入header变量中 
 * 根据引导扇区读取出来的内容计算根目录区的扇区个数
*/
void readBoot(FILE *fat12){
    struct Fat12Header* headptr = &header;
    fseek(fat12, 11, SEEK_SET); // BPB块从11字节开始
    fread(headptr, 1, sizeof(header), fat12);
    Rsvd_Sec = header.BPB_RsvdSecCnt;
    NumFATs = header.BPB_NumFATs;
    Byts_Per_Sec = header.BPB_BytsPerSec;
    Root_Ent_Cnt = header.BPB_RootEntCnt;
    Root_Sec = (Root_Ent_Cnt * ENT_SIZE + Byts_Per_Sec - 1) / Byts_Per_Sec;
    if (header.BPB_FATSz16 != 0) {
		FATSz = header.BPB_FATSz16;
	} else {
		FATSz = header.BPB_TotSec32;
	}
    Pre_Sec = 1 + NumFATs * FATSz; // 根目录前有多少扇区
}
/**确定文件的类型 
 * 1:普通文件/目录
 * 2:.
 * 3:..
*/
int identify(char *name){
    int flag = 1;
    //单独判断.和..
    if(name[0] == 0x2E){
        flag = 2;
        if(name[1] == 0x2E){
            flag = 3;
        }
        for(int i = 2;i<11;i++){
            if(name[i] != 0x20){
                return 0;
            }
        }
        return flag;
    }
    for(int i =0;i<11;i++){
        if(!((name[i] >= '0' && name[i] <= '9')
             || (name[i] >= 'a' && name[i] <= 'z')
             || (name[i] >= 'A' && name[i] <= 'Z')
             || name[i] == ' '|| name[i] == '_')){
            return 0;
        }
    }
    return flag;
}
/**
 * 从fat12文件系统中读取数据
 * 引导扇区加上FAT1和FAT2共19个扇区
 * 根目录区共14个扇区 ？
 * 递归实现
 */
void loadFiles(FILE *fat12, int father, int start, int ents){
    int l = curFiles;
    files[father].sub_pos = curFiles;
    struct RootEntry ent;
    struct RootEntry * entptr = &ent; 
    for (int i = 0; i < ents; i++){ 
        fseek(fat12, start+ENT_SIZE*i, SEEK_SET); 
        fread(entptr, 1, ENT_SIZE, fat12); // 读取32个字节，也就是一个条目
        if (entptr->attr == 0x10 || entptr->attr == 0x20) {
        // 0x10为目录，0x20为普通文件
            int id = identify(entptr->name);
            if (id==0) continue;
            if (id == 2 || id == 3){
                files[curFiles].name[0] = '.';
                files[curFiles].name[1] = 0x00;
                files[curFiles].fullname[0] = '.';
                files[curFiles].fullname[1] = 0x00;
                if (id == 3) {
                    files[curFiles].name[1] = '.';
                    files[curFiles].name[2] = 0x00;
                    files[curFiles].fullname[1] = '.';
                    files[curFiles].fullname[2] = 0x00;
                }
                files[curFiles].type = 0;
                files[curFiles].size = 0;
                files[curFiles].sub_pos = -1;
                files[curFiles].start_pos = 0;
            }
            else{
            //================================写入文件/目录名称=======================================    
                int t = (entptr->attr == 0x10)?0:1; // 是否为文件？1为文件，0为目录
                int j;
                for(j=0;j<8;j++){
                    if (entptr->name[j] == ' '){
                        break;
                    }
                    files[curFiles].name[j] = entptr->name[j];
                }
                int pos = j;
                if (t){
                    files[curFiles].name[pos++] = '.';
                    for (int j=8;j<11;j++){
                        if (entptr->name[j] != ' '){
                            files[curFiles].name[pos++] = entptr->name[j];
                        }
                    }
                }
                files[curFiles].name[pos] = 0x00;
                pos = 0;
                while (files[father].fullname[pos]) {
                    files[curFiles].fullname[pos] = files[father].fullname[pos];
                    pos++;
                }
                int cur = 0;
                while (files[curFiles].name[cur]){
                    files[curFiles].fullname[pos++] = files[curFiles].name[cur++];
                }
                if (t==0){
                    files[curFiles].fullname[pos++] = '/';
                }
                files[curFiles].fullname[pos] = 0x00;
            //=================================填入其余参数===========================================
                files[curFiles].size = t?entptr->fileSize:0;
                printf("%d", files[curFiles].size);
                files[curFiles].type = t;
                // 根目录区前扇区 + 根目录区扇区，簇号从2开始
                int stps = (Pre_Sec + Root_Sec + entptr->fstCluster - 2)*Byts_Per_Sec;
                files[curFiles].start_pos = stps;
                files[curFiles].cluster = entptr->fstCluster;
            }
            curFiles++;
        }
    }
    splitFiles();//当前目录已经读取完毕，处理子目录
    int r = curFiles;
    for (int i = l;i<r;i++){
        if (files[i].type == 0 && files[i].start_pos != 0){
        // 加载目录的子节点，但是要排除.和..
            loadFiles(fat12, i, files[i].start_pos, Byts_Per_Sec/ENT_SIZE); // 一般来说，最后一个参数等价于 512/32=16
        }
    }
}
void load(FILE *fat12){
// 递归的驱动函数
    readBoot(fat12);
    iniFiles();
    loadFiles(fat12, 0, Pre_Sec*Byts_Per_Sec, Root_Ent_Cnt);
}
/* 读取FAT内容 */
int getFATValue(FILE * fat12 , int num);
/* 在文件列表中寻找某个名称的文件 */
int locate(char* p, int size){
    int idx = -1;
    for (int i = 0; i< curFiles; i++){
        char * ptr = p;
        int j;
        for (j = 0; j < size;j++){
            if (*ptr == files[i].fullname[j]){
                ptr++;
            }
            else {
                break;
            }
        }
        if (j == size){
            idx = i;
            break;
        } 
    }
    return idx;
}
/* 寻找一个目录下的直接子目录和直接子文件数目 */
void findDirAndFile(int spos){
    int dir_n = 0;
    int file_n = 0;
    while (files[spos].type != -1){
        if (files[spos].type == 0){// 目录
            if (files[spos].sub_pos != -1) // 不计算.&..
                dir_n++;
        }else if (files[spos].type == 1){// 文件
            file_n++;
        }
        spos++;
    }
    char * blk = " ";
    my_print(blk, 1);
    printNum(dir_n);
    my_print(blk, 1);
    printNum(file_n);
}
/* ls指令的实现代码  */
void ls(int fpos){
    char * cnt = files[fpos].fullname; // cnt用来存储输入内容，可复用
    my_print(cnt, strlen(cnt));
    cnt = ":\n";
    my_print(cnt, strlen(cnt));
    int spos = files[fpos].sub_pos;
    int l = spos;
    while (files[spos].type != -1){
    // 一直遍历，直到分割符
        char * name = files[spos].name;
        if (files[spos].type == 1){
        // 输出文件名，白色
            my_print(name, strlen(name));
        }else if (files[spos].type == 0){
        // 输出目录
            change_to_red(); // 文件颜色：红
            my_print(name, strlen(name));
            back_to_default(); // 恢复颜色
        }
        if (files[spos+1].type != -1){
        // 下一个不是分割，说明还有文件，那就要输出空格
            cnt = " ";
            my_print(cnt, 1);  
        }
        spos++;    
    }
    cnt = "\n";
    my_print(cnt, 1);
    for (int i=l;i<spos;i++){
        if (files[i].type == 0 && files[i].start_pos != 0){
            ls(i);
        }
    }
}
/* ls -l指令的实现代码  */
void lsl(int fpos){
    char * cnt = files[fpos].fullname; // cnt用来存储输入内容，可复用
    my_print(cnt, strlen(cnt));
    findDirAndFile(files[fpos].sub_pos);
    cnt = ":\n";
    my_print(cnt, strlen(cnt));
    int spos = files[fpos].sub_pos;
    int l = spos;
    while (files[spos].type != -1){
    // 一直遍历，直到分割符
        char * name = files[spos].name;
        if (files[spos].type == 1){
        // 输出文件名 大小，白色
            my_print(name, strlen(name));
            char * cnt = " ";
            my_print(cnt, 1);
            printNum(files[spos].size);
        }else if (files[spos].type == 0){
        // 输出目录
            change_to_red(); // 文件颜色：红
            my_print(name, strlen(name));
            if (files[spos].sub_pos != -1){
            // 需要排除.&..
                findDirAndFile(files[spos].sub_pos);
            }
            back_to_default(); // 恢复颜色
        }
        cnt = "\n";
        my_print(cnt, 1);  
        spos++;    
    }
    cnt = "\n";
    my_print(cnt, 1);
    for (int i=l;i<spos;i++){
        if (files[i].type == 0 && files[i].start_pos != 0){
            ls(i);
        }
    }

}
/* cat指令的实现代码  */
void cat(FILE* fat12, int fpos){
    int start = files[fpos].start_pos;
    w nxtclst = getFATValue(fat12, files[fpos].cluster);
    fseek(fat12, start, SEEK_SET);
    char cnt[1024*1024*4];
    int len = fread(cnt, 1, Byts_Per_Sec, fat12);
    while (nxtclst < 0x0ff7){
    // nxtclst != 0x0ff7 && nxtclst < 0x0ff8
        start = (Pre_Sec + Root_Sec + nxtclst - 2)*Byts_Per_Sec;
        fseek(fat12, start, SEEK_SET);
        len += fread(cnt+len, 1, Byts_Per_Sec, fat12);
        nxtclst = getFATValue(fat12, nxtclst);
    }
    my_print(cnt, len);
    char *lf = "\n";
    my_print(lf, 1); // 最后打印换行符
}
/**************************************************************************************************************************************************
 * 命令读取区*/
char input[500]; // store a command, reusablee
int ptr = 0; // 输入的当前位置
int l = 0; // 是否设置-l选项
/**
 * read a command from the terminal
 * 0: error occur
 * 1: ls
 * 2: cat
 * -1: exit
 */
int readCommand(){
    fgets(input, 500, stdin);
    char cmd[10];
    int type; //-1 for exit, 1 for ls, and 2 for cat, a table-driven design
    while (input[ptr]!=' ' && input[ptr]!='\n' && input[ptr]){
        cmd[ptr] = input[ptr];
        ptr++;
    } // 取第一个字段的命令
    cmd[ptr] = '\0';
    if (cmd[0] == 'l' && cmd[1] == 's'){
        type = 1;
    }
    else if (cmd[0] == 'c' && cmd [1] == 'a' && cmd[2] == 't'){
        type = 2;
    }
    else if (cmd[0] == 'e' && cmd [1] == 'x' && cmd[2] == 'i' && cmd[3] == 't'){
        type = -1;
    }
    else {
        // 发出错误提示，但是程序本身不停止
        char * warn = "Command Wrong! Error:";
        printError(warn, strlen(warn), cmd, strlen(cmd));
        return 0; // 返回main，继续读取下一个指令
    }
    return type;
}
/**
 * read the argv
 * -l is only for ls
 * call the implements after parsing the inputs
 */
void loadArgv(int cmd, FILE* fat12){
    char dir[50]; // 参数中的目录(修正后的)，可重用
    char tmp[50]; // 暂存文件/目录参数
    int dnum = 0; // 输入的目录数，最多一个
    // 下面三个变量都为“错误选项输入报错”服务
    int ef = 0; // error flag, 1 means an error occured
    int mf = 0; // multi-files, 1 means more than one file
    int erri = 0; // error index
    int errn = 0; // error length
    int curr = 0; // 目录/文件字符串的当前长度
    while (input[ptr] && input[ptr] != '\n'){
        if (input[ptr]==' ') {
            ptr++;
            continue;
        }
        if (input[ptr] == '-'){
        // 输入选项
            erri = ptr;
            errn = 1;
            ptr++;
            if (cmd != 1) ef = 1;
            while (input[ptr] && input[ptr] != ' ' && input[ptr] != '\n'){
                if (input[ptr] != 'l'){
                    ef = 1;
                }
                ptr++;
                errn++;
            }
            if (erri + 1 == ptr){
                ef = 1;
            }//说明只输入了一个‘-’
            if (!ef) l = 1;
        }
        else {
        // 输入为正常参数
            if (dnum > 0){
                char * warn = "You input more than one file or directory!";
                char * cnt = "";
                printError(warn, strlen(warn), cnt, 0);
                mf = 1;
                break;
            }
            else {
            // 读取文件参数
                dnum += 1;
                while (input[ptr] && input[ptr] != ' ' && input[ptr] != '\n'){
                    tmp[curr++] = input[ptr++];
                }
            }
        }
        ptr++;
    }
    if (mf) return;
    if (ef){
        char * warn = "Wrong Input! The Option does not exist! Error:";
        printError(warn, strlen(warn), input + erri, errn);
        return;
    }
    int start = -1; // 除了. /，目录开始的索引
    dir[0] = '/';
    for (int i = 0;i<curr;i++){
        if (tmp[i]!='.'&&tmp[i]!='/'){
            start = i;
            break;
        }
    }
    int dirlen = 1; // 最终目录的长度
    if (start > -1){
    /**
     * start只可能在三种情况下仍然等于初始值-1，那就是输入
     * / 表示根目录
     * ./和.，表示当前目录
     * 这三种情况什么都不用做，因为最终目录都是‘/’
    */
        for (int i=start; i<curr;i++){
            dir[dirlen++] = tmp[i];
        }
    }
    if (cmd == 1) {
    //ls指令的目标是目录，所以需要补全目录，即保证以‘/’结尾
        if (curr>0 && tmp[curr-1]!='/'){
            dir[dirlen++] = '/';
        }
    }
    dir[dirlen] = '\0'; // 保险起见加上终止符
    //根据指令调度实现
    int fpos = locate(dir, dirlen);
    if (fpos == -1){
    // 没有找到文件，报错
        char * warn = "File or directory does not exist!";
        char * cnt = "";
        printError(warn, strlen(warn), cnt, 0);
        return;
    }
    else {
    // 使用文件索引作为参数调用实现
        if (cmd==1) {
            if (l) {
                lsl(fpos);
            }
            else {
                ls(fpos);
            }
        }
        else if(cmd==2){
            cat(fat12, fpos);
        }
        else {
            char * err = "A bad error has happened.";
            my_print(err, strlen(err));
        }
    }
}
// 程序入口，执行一个循环，只有在接收到用户的“exit”指令后才会退出
int main(){
    FILE *fat12 = fopen("a.img", "rb"); // 打开文件系统
    load(fat12); // 导入文件，所有文件被填入数组数据结构
    while (1){
        my_print(">", 1);
        ptr = 0; //读取新的输入时，游标归0
        l = 0;
        int code = readCommand();
        if (code == -1){
            char * gb = "Goodbye!\n";
            my_print(gb, strlen(gb));
            break; 
        }// the only way to break the loop
    // ==========================load arguments===================
        if (code){ // code == 0 means a error code, there is no need to read the left input
            loadArgv(code, fat12); // 读入参数，根据输入参数调用不同的实现
        }
    }
    fclose(fat12);
}
/* 获取第num个FAT项 */
int getFATValue(FILE * fat12 , int num) {
	//FAT1的偏移字节
	int fatBase = Rsvd_Sec * Byts_Per_Sec;
	//FAT项的偏移字节
	int fatPos = fatBase + num*3/2;
	//奇偶FAT项处理方式不同，分类进行处理，从0号FAT项开始, 0偶奇1
	int type = 0;
	if (num % 2 != 0) {
		type = 1;
	}
	//先读出FAT项所在的两个字节
	w bytes;
	w* bytes_ptr = &bytes;
	int check;
	fseek(fat12, fatPos, SEEK_SET);
    fread(bytes_ptr , 1, 2, fat12); // 读入2字节
	//type为0的话，取byte2的低4位和byte1构成的值，type为1的话，取byte2和byte1的高4位构成的值
	if (type == 0) {
		return bytes & 0x0fff; //低12位
	} else {
		return bytes>>4; // 高12位
	}
}