//
// Virual Memory Simulator
// One-level page table system with FIFO and LRU
// Two-level page table system with LRU
// Inverted page table with a hashing system 
// Name: Kim Young Min

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define PAGESIZEBITS 12			// page size = 4Kbytes
#define VIRTUALADDRBITS 32		// virtual address space size = 4Gbytes

struct pageTableEntry {
	int pid;
	struct pageTableEntry *right;
	struct pageTableEntry *left;
	int level;				// page table level (1 or 2)
	char valid;
	struct pageTableEntry *secondLevelPageTable;	// valid if this entry is for the first level page table (level = 1)
	int frameNumber;				// valid if this entry is for the second level page table (level = 2)
	unsigned int vpn;
};

struct framePage {
	int number;			// frame number
	int pid;			// Process id that owns the frame
	int virtualPageNumber;			// virtual page number using the frame
	struct framePage *lruLeft;	// for LRU circular doubly linked list
	struct framePage *lruRight; // for LRU circular doubly linked list
	struct framePage *LRU;
};

struct invertedPageTableEntry {
	int pid;					// process id
	int virtualPageNumber;		// virtual page number
	int frameNumber;			// frame number allocated
	struct invertedPageTableEntry *next;
};

struct procEntry {
	char *traceName;			// the memory trace name
	int pid;					// process (trace) id
	int ntraces;				// the number of memory traces
	int num2ndLevelPageTable;	// The 2nd level page created(allocated);
	int numIHTConflictAccess; 	// The number of Inverted Hash Table Conflict Accesses
	int numIHTNULLAccess;		// The number of Empty Inverted Hash Table Accesses
	int numIHTNonNULLAccess;		// The number of Non Empty Inverted Hash Table Accesses
	int numPageFault;			// The number of page faults
	int numPageHit;				// The number of page hits
	struct pageTableEntry *firstLevelPageTable;
	//struct pageTableEntry *secondLevelPageTable;
	FILE *tracefp;
};

struct framePage *oldestFrame; // the oldest frame pointer
int firstLevelBits, phyMemSizeBits, numProcess;
int s_flag = 0;

void initPhyMem(struct framePage *phyMem, int nFrame) {
	int i;
	for(i = 0; i < nFrame; i++) {
		phyMem[i].number = i;
		phyMem[i].pid = -1;
		phyMem[i].virtualPageNumber = -1;
		phyMem[i].lruLeft = &phyMem[(i-1+nFrame) % nFrame];
		phyMem[i].lruRight = &phyMem[(i+1+nFrame) % nFrame];
	}

	oldestFrame = &phyMem[0];

}

void initProcTable(struct procEntry *procTable, char *proname[],int numProcess)
{
	int i,j;
	int Size = 1;
	for (j = 0; j < 20; j++) //onelevel���� ���������̺� ������ ����
	{
		Size = Size * 2; // 2�� 20��
	}
	for (i = 0; i < numProcess; i++)
	{
		procTable[i].traceName = proname[i];
		procTable[i].pid = i;
		procTable[i].ntraces = 0;
		procTable[i].num2ndLevelPageTable = 0;
		procTable[i].numPageFault = 0;
		procTable[i].numPageHit = 0;
		procTable[i].numIHTConflictAccess = 0;
		procTable[i].numIHTNULLAccess = 0;
		procTable[i].numIHTNonNULLAccess = 0;
		procTable[i].tracefp = fopen(proname[i], "r");
		procTable[i].firstLevelPageTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry)*(Size));
		for (j = 0; j < Size; j++)
		{
			procTable[i].firstLevelPageTable[j].level = 1;
			procTable[i].firstLevelPageTable[j].valid = 0;
			procTable[i].firstLevelPageTable[j].secondLevelPageTable = NULL;
			
		}
	}
}
void initProcTable2(struct procEntry *procTable, char *proname[], int numProcess)
{
	int i, j;
	int size = 1;
	for (j = 0; j < firstLevelBits; j++) //1st ���������̺� ũ��
	{
		size = size * 2; 
	}
	j = 0;
	for (i = 0; i < numProcess; i++)
	{
		procTable[i].traceName = proname[i];
		procTable[i].pid = i;
		procTable[i].ntraces = 0;
		procTable[i].num2ndLevelPageTable = 0;
		procTable[i].numPageFault = 0;
		procTable[i].numPageHit = 0;
		procTable[i].numIHTConflictAccess = 0;
		procTable[i].numIHTNULLAccess = 0;
		procTable[i].numIHTNonNULLAccess = 0;
		procTable[i].tracefp = fopen(proname[i], "r");
		procTable[i].firstLevelPageTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry)*(size));
		for (j = 0; j < size; j++)
		{
			procTable[i].firstLevelPageTable[j].level = 1;
			procTable[i].firstLevelPageTable[j].valid = 0;
			procTable[i].firstLevelPageTable[j].secondLevelPageTable = NULL;
		}
	}
}

void oneLevelVMSim(struct procEntry *procTable, struct framePage *phyMemFrames, char FIFOorLRU, char *argv[]) { //fifo����
	int i, trcount;
	unsigned int Vaddr, Paddr;
	int size = 1;
	char rw;
	unsigned int oldvpn, oldpid;
	unsigned int frnum;
	int trnum = 0;
	unsigned int temp1, temp2, temp3;
	trcount = 0;
	for (i = 0; i < numProcess; ++i) {
		while (!feof(procTable[i].tracefp)) {
			fscanf(procTable[i].tracefp, "%x %c", &Vaddr, &rw);
			trcount++;
		}
		trcount--;
		procTable[i].ntraces =trcount;
		
		trcount = 0;
		fclose(procTable[i].tracefp);
	}
	for (i = 0; i < numProcess; ++i) {
		if (trnum < procTable[i].ntraces)trnum = procTable[i].ntraces;
	}

	for (i = 0; i < numProcess; ++i) {
		procTable[i].tracefp = fopen(procTable[i].traceName, "r");
	}
	while (trnum--) { 
		trcount++;
		for (i = 0; i < numProcess; i++) {
			fscanf(procTable[i].tracefp, "%x %c", &Vaddr, &rw);
			temp1 = Vaddr >> PAGESIZEBITS; // �����ּ��� ���� 20��Ʈ
			temp2 = Vaddr << 20;
			temp2 = temp2 >> 20; // ������ ���� 12��Ʈ
			procTable[i].ntraces = trcount; //Ʈ���̽� �� �� ° �а� �ִ���
			if (FIFOorLRU == 'L') { //LRU���
				if (procTable[i].firstLevelPageTable[temp1].valid == 0) {//��������Ʈ
					procTable[i].numPageFault++;
					procTable[i].firstLevelPageTable[temp1].valid = 1;
					procTable[i].firstLevelPageTable[temp1].frameNumber = oldestFrame->number;
					if (oldestFrame->pid != -1) { //�����ø޸𸮰� ���� ä�����ִٸ�
						oldvpn = oldestFrame->virtualPageNumber;
						oldpid = oldestFrame->pid;
						procTable[oldpid].firstLevelPageTable[oldvpn].valid = 0;
						oldestFrame->virtualPageNumber = temp1;
						oldestFrame->pid = procTable[i].pid;				
						oldestFrame = oldestFrame->lruRight;
					}
					else { //�����ø޸𸮰� �������ʾ������
						oldestFrame->virtualPageNumber = temp1;
						oldestFrame->pid = procTable[i].pid;
						oldestFrame = oldestFrame->lruRight;
					}
					temp3 = procTable[i].firstLevelPageTable[temp1].frameNumber << PAGESIZEBITS;
					Paddr = temp3 | temp2;
					if (argv[1][0] == '-' && argv[1][1] == 's') {
						printf("One-Level procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces, Vaddr, Paddr);
					}
				}
				else {//��Ʈ�������
					procTable[i].numPageHit++;
					frnum = procTable[i].firstLevelPageTable[temp1].frameNumber;
					if (&phyMemFrames[frnum] == oldestFrame) {
						oldestFrame = oldestFrame->lruRight;
					}
					else { //���� ���� ��� ���� ���� �� ��ü
						phyMemFrames[frnum].lruLeft->lruRight = phyMemFrames[frnum].lruRight;
						phyMemFrames[frnum].lruRight->lruLeft = phyMemFrames[frnum].lruLeft;
						oldestFrame->lruLeft->lruRight = &phyMemFrames[frnum];
						phyMemFrames[frnum].lruRight = oldestFrame;
						phyMemFrames[frnum].lruLeft = oldestFrame->lruLeft;
						oldestFrame->lruLeft = &phyMemFrames[frnum];
					}
					temp3 = procTable[i].firstLevelPageTable[temp1].frameNumber << PAGESIZEBITS;
					Paddr = temp3 | temp2;
					if (argv[1][0] == '-' && argv[1][1] == 's') {
						printf("One-Level procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces, Vaddr, Paddr);
					}
				}
			}
			if (FIFOorLRU == 'F') {
				if (procTable[i].firstLevelPageTable[temp1].valid == 1) { //�븮�� ��Ʈ�� 1�̴ϱ� �ش� Ʈ���̽� ���������̺��� �ְڱ���
				//�����ϱ� �ּҺ�ȯ ����߰���?
					procTable[i].numPageHit++; //��Ʈ����Ʈ!
					oldestFrame->LRU = oldestFrame->lruRight; // �ֱٿ� ����� ������ �������� ���� ���� ������ ģ��
					temp1 = Vaddr >> PAGESIZEBITS; // �����ּ��� ���� 20��Ʈ
					temp2 = Vaddr << 20;
					temp2 = temp2 >> 20; // ������ ���� 12��Ʈ
					temp3 = procTable[i].firstLevelPageTable[temp1].frameNumber << PAGESIZEBITS;
					Paddr = temp3 | temp2;
					if (argv[1][0] == '-' && argv[1][1] == 's') {
						printf("One-Level procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces, Vaddr, Paddr);
					}
				}
				else if (procTable[i].firstLevelPageTable[temp1].valid == 0) {
					//�븮�尡 0�̴ϱ� ��Ʈ��?
					procTable[i].numPageFault++;
					procTable[i].firstLevelPageTable[temp1].valid = 1; // ������ �븮�� ��Ʈ�� 1�� �ž���
					if (oldestFrame->pid == -1) { // �׷��ٸ� �����ø޸𸮿� �� ���� �ʾ��� ��� ���� �غ���?
						oldestFrame->virtualPageNumber = temp1;
						procTable[i].firstLevelPageTable[temp1].frameNumber = oldestFrame->number;
						oldestFrame->pid = i; //�������� ���μ��� ��ȣ
						//oldestFrame->LRU = oldestFrame;
						oldestFrame = oldestFrame->lruRight; //pid�� -1�� �����ø޸� ���� �ϳ��� oldestFrame�� �о���� ��ũ�帮��Ʈ ����
						temp1 = Vaddr >> PAGESIZEBITS; // �����ּ��� ���� 20��Ʈ
						temp2 = Vaddr << 20;
						temp2 = temp2 >> 20; // ������ ���� 12��Ʈ
						temp3 = procTable[i].firstLevelPageTable[temp1].frameNumber << PAGESIZEBITS;
						Paddr = temp3 | temp2;
						if (argv[1][0] == '-' && argv[1][1] == 's') {
							printf("One-Level procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces, Vaddr, Paddr);
						}
					}
					else { //�����ø޸𸮰� �� á, ���÷��̽� ���
						//oldestFrame = oldestFrame->lruRight; 
						// ó������ ���ƿ��� ä������ ������� ��ü �� ����, ���� ����Ʈ
						oldestFrame->pid = i; //�������� ���μ��� ��ȣ �ֽ�ȭ
						temp1 = Vaddr >> PAGESIZEBITS; // �����ּ��� ���� 20��Ʈ
						temp2 = Vaddr << 20;
						temp2 = temp2 >> 20; // ������ ���� 12��Ʈ
						procTable[i].firstLevelPageTable[temp1].frameNumber = oldestFrame->number; // ������ �ѹ� ���
						oldvpn = oldestFrame->virtualPageNumber; // ���� ����Ű�� �����ּ� ã�ư��� ���� �ε���
						procTable[i].firstLevelPageTable[oldvpn].valid = 0; // ���� ����Ű�� �ִ� ���������̺��� �ּ� �븮���Ʈ�� �ƽ��Ե� 0�̵� ��.��
						oldestFrame->virtualPageNumber = temp1; // ������ ���� �ּ� �ֽ�ȭ
						//procTable[i].firstLevelPageTable->valid = 1; // ���� �븮�� ��Ʈ�� 1�� �Ǳ⿡ ����ϴ� ������ �̹� ��
						oldestFrame = oldestFrame->lruRight;
						temp3 = procTable[i].firstLevelPageTable[temp1].frameNumber << PAGESIZEBITS;
						Paddr = temp3 | temp2;
						if (argv[1][0] == '-' && argv[1][1] == 's') {
							printf("One-Level procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces, Vaddr, Paddr);
						}
					}
				}
			}
		}
	}
	for (i = 0; i < numProcess; i++) {
		printf("**** %s *****\n", procTable[i].traceName);
		printf("Proc %d Num of traces %d\n", i, procTable[i].ntraces);
		printf("Proc %d Num of Page Faults %d\n", i, procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n", i, procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
	}
}
void twoLevelVMSim(struct procEntry *procTable, struct framePage *phyMemFrames, char *argv[]) {
	int i, j, trcount;
	unsigned int Vaddr, Paddr;
	int size = 1;
	char rw;
	unsigned int oldvpn,oldpid,frnum;
	int trnum = 0;
	unsigned int temp1, temp2, temp3, temp4, temp5, temp6;
	unsigned int oldftemp, oldstemp, temp7;
	trcount = 0;
	for (i = 0; i < 20 - firstLevelBits; ++i)size = size * 2; //�������� ���������̺�
	for (i = 0; i < numProcess; ++i) {
		while (!feof(procTable[i].tracefp)) {
			fscanf(procTable[i].tracefp, "%x %c", &Vaddr, &rw);
			trcount++;
		}
		trcount--;
		procTable[i].ntraces = trcount;
		
		trcount = 0;
		fclose(procTable[i].tracefp);
	}
	for (i = 0; i < numProcess; ++i) {
		if (trnum < procTable[i].ntraces)trnum = procTable[i].ntraces;
	}
	for (i = 0; i < numProcess; ++i) {
		procTable[i].tracefp = fopen(procTable[i].traceName, "r");
	}
	while (trnum--){
		trcount++;
		for(i = 0; i < numProcess; ++i) {
			//hit���� ��, fault���µ� �����ø޸𸮰� �η��ؼ� ä��⸸�ϸ�ɶ��� �����ø޸𸮰� �� ���� ���÷��̽� �ؾ��Ҷ�
			fscanf(procTable[i].tracefp, "%x %c", &Vaddr, &rw);
			temp1 = Vaddr >> (32-firstLevelBits); // �����ּ��� firstLevelBit��ŭ ���� ������� 24��Ʈ�а� 8��Ʈ ����
			temp2 = Vaddr << firstLevelBits; // flb�� 8 �̾����� 32-8-12 ��������12��Ʈ �����Ϸ��� �۾�, �ϴ� �������� 8��Ʈ�а�
			temp3 = temp2 >> (PAGESIZEBITS + firstLevelBits); //�������������Ʈ12 + flb8��ŭ �ٽùо 12��Ʈ�� ������ ����������Ʈ����
			temp4 = Vaddr >> PAGESIZEBITS; // ����20��Ʈ ������
			temp5 = Vaddr << (32 - PAGESIZEBITS); //������ ����12��Ʈ�� ���ϱ� ���� �۾�
			temp6 = temp5 >> (32 - PAGESIZEBITS); //������ ���� 12��Ʈ
			procTable[i].ntraces = trcount; //Ʈ���̽� �� �� ° �а� �ִ���
			if (procTable[i].firstLevelPageTable[temp1].valid == 0) { //�۽�Ʈ���� ��������Ʈ
				procTable[i].numPageFault++;
				procTable[i].firstLevelPageTable[temp1].valid = 1;
				procTable[i].num2ndLevelPageTable++;
				procTable[i].firstLevelPageTable[temp1].secondLevelPageTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry)*(size));
				for (j = 0; j < size; ++j) {
					procTable[i].firstLevelPageTable[temp1].secondLevelPageTable[j].valid = 0;
					procTable[i].firstLevelPageTable[temp1].secondLevelPageTable[j].level = 2;
				}
				procTable[i].firstLevelPageTable[temp1].secondLevelPageTable[temp3].valid = 1;
				procTable[i].firstLevelPageTable[temp1].secondLevelPageTable[temp3].frameNumber = oldestFrame->number;
				if (oldestFrame->pid != -1) { //�����ø޸𸮰� ä�����ִٸ�
					oldvpn = oldestFrame->virtualPageNumber;
					oldpid = oldestFrame->pid;
					oldftemp = oldvpn >> (20 - firstLevelBits); //����������Ʈ��ŭ �о������ �۽�Ʈ������Ʈ��ŭ �ּҳ���
					temp7 = oldvpn << (PAGESIZEBITS + firstLevelBits); //12+�۽�Ʈ������ŭ �������� �о������ ���������ּҸ�ŭ�� ���ʿ� �о������� �ٽÿ��������� ���ܾ�
					oldstemp = temp7 >> (PAGESIZEBITS + firstLevelBits); // ����������Ʈ��ŭ �ּҳ���
					procTable[oldpid].firstLevelPageTable[oldftemp].secondLevelPageTable[oldstemp].valid = 0; //��ü
					oldestFrame->virtualPageNumber = temp4; //���� 20��Ʈ, �����ּ�
					oldestFrame->pid = procTable[i].pid;
					oldestFrame = oldestFrame->lruRight; //���������� ��������
				}
				else { //oldestFrame->pid == -1; �����ø޸𸮰� ������ִٸ�
					oldestFrame->virtualPageNumber = temp4;
					oldestFrame->pid = procTable[i].pid;
					oldestFrame = oldestFrame->lruRight; //���������� ��������
				}
				temp5 = procTable[i].firstLevelPageTable[temp1].secondLevelPageTable[temp3].frameNumber << PAGESIZEBITS;//�����°���Ȯ��
				Paddr = temp5 | temp6;
				if (argv[1][0] == '-' && argv[1][1] == 's') {
					printf("Two-Level procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces, Vaddr, Paddr);
				}
			}
			else {//�۽�Ʈ���������� ��Ʈ���� �����巹������ ��Ʈ�϶� Ȥ�� �۽�Ʈ���� �����巹���Ѵ� ��Ʈ
				//�����巹������ ��Ʈ�ϱ� �ᱹ ��������Ʈ ����
				if (procTable[i].firstLevelPageTable[temp1].secondLevelPageTable[temp3].valid == 0) {
					procTable[i].numPageFault++;
					procTable[i].firstLevelPageTable[temp1].secondLevelPageTable[temp3].valid = 1;
					procTable[i].firstLevelPageTable[temp1].secondLevelPageTable[temp3].frameNumber = oldestFrame->number;
					if (oldestFrame->pid != -1) { //�����ø޸𸮰� ä�����ִٸ�
						oldvpn = oldestFrame->virtualPageNumber;
						oldpid = oldestFrame->pid;
						oldftemp = oldvpn >> (20 - firstLevelBits); //����������Ʈ��ŭ �о������ �۽�Ʈ������Ʈ��ŭ �ּҳ���
						temp7 = oldvpn << (PAGESIZEBITS + firstLevelBits); //12+�۽�Ʈ������ŭ �������� �о������ ���������ּҸ�ŭ�� ���ʿ� �о������� �ٽÿ��������� ���ܾ�
						oldstemp = temp7 >> (PAGESIZEBITS + firstLevelBits); // ����������Ʈ��ŭ �ּҳ���
						procTable[oldpid].firstLevelPageTable[oldftemp].secondLevelPageTable[oldstemp].valid = 0;
						oldestFrame->virtualPageNumber = temp4; //���� 20��Ʈ, �����ּ�
						oldestFrame->pid = procTable[i].pid;
						oldestFrame = oldestFrame->lruRight; //���������� ��������
					}
					else {
						oldestFrame->virtualPageNumber = temp4;
						oldestFrame->pid = procTable[i].pid;
						oldestFrame = oldestFrame->lruRight; //���������� ��������
					}
				}
				else { // �� �� ��Ʈ
					procTable[i].numPageHit++;
					frnum = procTable[i].firstLevelPageTable[temp1].secondLevelPageTable[temp3].frameNumber;
					if (&phyMemFrames[frnum] == oldestFrame) {
						oldestFrame = oldestFrame->lruRight;
					}
					else { //���� �������� ����Ѱ� ��ü
						phyMemFrames[frnum].lruLeft->lruRight = phyMemFrames[frnum].lruRight;
						phyMemFrames[frnum].lruRight->lruLeft = phyMemFrames[frnum].lruLeft;
						oldestFrame->lruLeft->lruRight = &phyMemFrames[frnum];
						phyMemFrames[frnum].lruRight = oldestFrame;
						phyMemFrames[frnum].lruLeft = oldestFrame->lruLeft;
						oldestFrame->lruLeft = &phyMemFrames[frnum];
					}     
				}
				temp5 = procTable[i].firstLevelPageTable[temp1].secondLevelPageTable[temp3].frameNumber << PAGESIZEBITS;//�����°���Ȯ��
				Paddr = temp5 | temp6;
				if (argv[1][0] == '-' && argv[1][1] == 's') {
					printf("Two-Level procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces, Vaddr, Paddr);
				}
			}
		}
		
	}
	
	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of second level page tables allocated %d\n",i,procTable[i].num2ndLevelPageTable);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
	}
}

void invertedPageVMSim(struct procEntry *procTable, struct framePage *phyMemFrames, int nFrame, char *argv[]) {
	int i, trcount;
	unsigned int Vaddr, Paddr;
	int size = 1;
	char rw;
	unsigned int oldvpn, oldhindex, currenthindex;
	struct pageTableEntry *frnum;
	struct pageTableEntry *frnum2;
	unsigned int frnum3;
	int trnum = 0;
	unsigned int temp1, temp2, temp3;
	trcount = 0;
	struct pageTableEntry *HT = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry)*nFrame);
	for (i = 0; i < nFrame; ++i) {
		HT[i].valid = 0;
		HT[i].right = NULL;
		HT[i].left = NULL;
		HT[i].pid = -1;
	}
	for (i = 0; i < numProcess; ++i) {
		while (!feof(procTable[i].tracefp)) {
			fscanf(procTable[i].tracefp, "%x %c", &Vaddr, &rw);
			trcount++;
		}
		trcount--;
		procTable[i].ntraces = trcount;
		trcount = 0;
		fclose(procTable[i].tracefp);
	}
	for (i = 0; i < numProcess; ++i) {
		if (trnum < procTable[i].ntraces)trnum = procTable[i].ntraces;
	}
	for (i = 0; i < numProcess; ++i) {
		procTable[i].tracefp = fopen(procTable[i].traceName, "r");
	}
	while (trnum--){
		trcount++;
		for (i = 0; i < numProcess; ++i) {
			fscanf(procTable[i].tracefp, "%x %c", &Vaddr, &rw);
			temp1 = Vaddr >> PAGESIZEBITS; // �����ּ��� ���� 20��Ʈ
			currenthindex = (temp1 + procTable[i].pid) % nFrame; //Hashtable index
			temp2 = Vaddr << (32 - PAGESIZEBITS);
			temp2 = temp2 >> (32 - PAGESIZEBITS); // ������ ���� 12��Ʈ
			//trcount++;
			procTable[i].ntraces = trcount; //Ʈ���̽� �� �� ° �а� �ִ���
			if (HT[currenthindex].right == NULL) {  //�ؽð� ä���� ���� ���� ���̺�
				procTable[i].numPageFault++;
				procTable[i].numIHTNULLAccess++; //�ξ׼��� 1����
				HT[currenthindex].valid = 1; //���� �븮���Ʈ�� 1�� �ʱ�ȭ
				//�ؽ����̺��� ���������� ��� ���� �Ҵ�
				HT[currenthindex].right = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry));
				//���� ����
				HT[currenthindex].right->pid = procTable[i].pid;
				HT[currenthindex].right->vpn = temp1;
				HT[currenthindex].right->frameNumber = oldestFrame->number; //pid�� -1�� �����ø޸� ��ġ
				//���������� �Ҵ����� ����� �������� ���� �ƹ��͵� ����Ű�� �ʰ�, left�� �̾��ִ� �۾� �ʿ�
				HT[currenthindex].right->left = &HT[currenthindex];
				HT[currenthindex].right->right = NULL;
				if (oldestFrame->pid != -1) {//�����ø޸𸮰� ���� �� �ִٸ�
					oldvpn = oldestFrame->virtualPageNumber;
					oldhindex = (oldvpn + (oldestFrame->pid)) % nFrame;
					frnum = HT[oldhindex].right; //�˻��� ����
					while (frnum) { //NULL�� �ƴ� ������ ���������� �˻�
						if (frnum->pid == oldestFrame->pid && frnum->vpn == oldvpn) {
							//�õ��ؽ����̺��� �����ʳ���� ���μ������̵�� �õ�Ʈ�������� �Ǿ��̵� ����
							//�õ��ؽ����̺� ��������� �����ּҿ� �õ�Ʈ������ �����ּҿ� ��ġ�Ѵٸ�
							//�õ��ؽ����̺� ����� ����Ʈ�����͸� <����> �õ��ؽ����̺��� ��ĭ ������ ��忡 �̾���
							//�����ø޸𸮿��� ���÷��̽� �Ǳ� ������ �ؽ����̺������� �� �������� ����
							frnum->left->right = frnum->right;
							if (frnum->right != NULL) { //�õ��������� ����Ʈ�����Ͱ� ���� �ƴ϶��, ���� �ƴҰ��
								frnum->right->left = frnum->left; //������ �̾��� ����� ����Ʈ�����͵� ���� �̾���
							}
							break;
						}
						frnum = frnum->right; //���������� �̵��ϸ鼭 �̾��� ���� �˻�
					}
					oldestFrame->virtualPageNumber = temp1;
					oldestFrame->pid = procTable[i].pid;
					oldestFrame = oldestFrame->lruRight;
				}
				else { //�����ø޸𸮰� ���� �� ���� ���� ���
					oldestFrame->virtualPageNumber = temp1;
					oldestFrame->pid = procTable[i].pid;
					oldestFrame = oldestFrame->lruRight; //�õ�Ʈ������ �ѱ�� �۾� �״�� ���ָ� ��
				}
				temp3 = HT[currenthindex].right->frameNumber << PAGESIZEBITS;
				Paddr = temp3 | temp2;
				if (argv[1][0] == '-' && argv[1][1] == 's') {
					printf("IHT procID procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces, Vaddr, Paddr);
				}
				
			}//�ؽ����̺� �ε����� �ƹ��͵� ������� ������� ��
			else { // ����������, �ؽ����̺��� �� �ִٸ�
				procTable[i].numIHTNonNULLAccess++;
				frnum2 = HT[currenthindex].right; //���� �ؽ����̺��ε����� ����Ű�� �ִ� �޸��� ���̺�
				while (frnum2) { //���������� �ѱ�鼭 �˻��Ұ���
					procTable[i].numIHTConflictAccess++;
					if (frnum2->vpn == temp1 && frnum2->pid == i) {//���� ��ġ ������ ��Ʈ
						procTable[i].numPageHit++;
						frnum3 = frnum2->frameNumber;
						//�ؽ����̺��� ���� �����ø޸� ã�� �� ����
						if (&phyMemFrames[frnum3] == oldestFrame) {
							oldestFrame = oldestFrame->lruRight;
							temp3 = frnum2->frameNumber << PAGESIZEBITS;
							//temp3 = frnum3 << PAGESIZEBITS;
							Paddr = temp3 | temp2;
							if (argv[1][0] == '-' && argv[1][1] == 's') {
								printf("IHT procID procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces, Vaddr, Paddr);
							}
							break;
						}
						else { //oldestFrame�� �̿��� LRU����
						
							phyMemFrames[frnum3].lruLeft->lruRight = phyMemFrames[frnum3].lruRight;
							phyMemFrames[frnum3].lruRight->lruLeft = phyMemFrames[frnum3].lruLeft;
							oldestFrame->lruLeft->lruRight = &phyMemFrames[frnum3];
							phyMemFrames[frnum3].lruRight = oldestFrame;
							phyMemFrames[frnum3].lruLeft = oldestFrame->lruLeft;
							oldestFrame->lruLeft = &phyMemFrames[frnum3];
							//temp3 = frnum2->frameNumber << PAGESIZEBITS;
							temp3 = frnum2->frameNumber << PAGESIZEBITS;
							Paddr = temp3 | temp2;
							if (argv[1][0] == '-' && argv[1][1] == 's') {
								printf("IHT procID procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces, Vaddr, Paddr);
							}
							break;
						}
					}
					frnum2 = frnum2->right;
				} //�ؽ����̺��� �� �ְ�, ������ �˻��ߴµ� ����
				// ��, ��Ʈ
				if (frnum2 == NULL) { //���� ��� �߰� �� ��� ��
					procTable[i].numPageFault++;
					frnum2 = HT[currenthindex].right; 
					frnum = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry));
					frnum->frameNumber = oldestFrame->number;
					frnum->pid = i;
					frnum->vpn = temp1;
					frnum->right = frnum2;
					frnum->left = frnum2->left;
					frnum2->left->right = frnum;
					frnum2->left = frnum;
					temp3 = frnum->frameNumber << PAGESIZEBITS;
					Paddr = temp3 | temp2;
					if (argv[1][0] == '-' && argv[1][1] == 's') {
						printf("IHT procID procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces, Vaddr, Paddr);
					}
					//�õ�Ʈ���������� ������ �޸� Ȯ��
					if (oldestFrame->pid != -1) {//�����ø޸𸮰� ���� �� �ִٸ�
						//���� ���� ���� �ִ� �߰����ֵ� ��ü�Ǿ����� ���̺��� �ؽø� �����������
						oldvpn = oldestFrame->virtualPageNumber;
						oldhindex = (oldvpn + oldestFrame->pid) % nFrame;
						frnum = HT[oldhindex].right;//�˻��� ���� �õ��ε����� ������ ��带 ����Ŵ
						while (frnum) {//NULL�� �ƴ� ������ ���������� �˻�
							if (frnum->pid == oldestFrame->pid && frnum->vpn == oldvpn) {
								//�õ��ؽ����̺��� �����ʳ���� ���μ������̵�� �õ�Ʈ�������� �Ǿ��̵� ����
								//�õ��ؽ����̺� ��������� �����ּҿ� �õ�Ʈ������ �����ּҿ� ��ġ�Ѵٸ�
								//�õ��ؽ����̺� ����� ����Ʈ�����͸� <����> �õ��ؽ����̺��� ��ĭ ������ ��忡 �̾���
								//�� ���÷��̽� �Ǿ����� �ؽø� �� ������
								frnum->left->right = frnum->right;
								if (frnum->right != NULL) {//�õ��������� ����Ʈ�����Ͱ� ���� �ƴ϶��, ���� �ƴҰ��
									frnum->right->left = frnum->left;//������ �̾��� ����� ����Ʈ�����͵� ���� �̾���
								}
								break;
							}
							frnum = frnum->right;//���������� �̵��ϸ鼭 �̾��� ���� �˻�
						}
						oldestFrame->virtualPageNumber = temp1;
						oldestFrame->pid = procTable[i].pid;
						oldestFrame = oldestFrame->lruRight;
					}
					else {//�����ø޸𸮰� ���� �� ���� ���� ���
						oldestFrame->virtualPageNumber = temp1;
						oldestFrame->pid = procTable[i].pid;
						oldestFrame = oldestFrame->lruRight;
					}
				}
			}
		}
	}
		
	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of Inverted Hash Table Access Conflicts %d\n",i,procTable[i].numIHTConflictAccess);
		printf("Proc %d Num of Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNULLAccess);
		printf("Proc %d Num of Non-Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNonNULLAccess);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
		assert(procTable[i].numIHTNULLAccess + procTable[i].numIHTNonNULLAccess == procTable[i].ntraces);
	}
}

int main(int argc, char *argv[]) {
	int i,j;
	int DramSize,DramSize2;
	DramSize = 1;
	DramSize2 = 1;
	/*
	if (argc<5) { //���߿� ����
	     printf("Usage : %s [-s] simType firstLevelBits PhysicalMemorySizeBits TraceFileNames\n",argv[0]);
	}*/
	numProcess = 0;
	// memsim [-s] simType firstLevelBits PhysicalMemorySizeBits TraceFileNames
	if (argv[1][0] == '-' && argv[1][1] == 's') {
		if(argc<6){
		printf("Usage : %s [-s] simType firstLevelBits PhysicalMemorySizeBIts TraceFileNames\n",argv[0]);
		exit(1);
		}
		firstLevelBits = atoi(argv[3]);
		phyMemSizeBits = atoi(argv[4]);
		if (phyMemSizeBits < PAGESIZEBITS) {
			printf("PhysicalMemorySizeBits %d should be larger than PageSizeBits %d\n", phyMemSizeBits, PAGESIZEBITS); exit(1);
		}
		if (VIRTUALADDRBITS - PAGESIZEBITS - firstLevelBits <= 0) {
			printf("firstLevelBits %d is too Big for the 2nd level page system\n", firstLevelBits); exit(1);
		}
		numProcess = argc - 5;
		for (i = 0; i < phyMemSizeBits - 12; ++i) {
			DramSize = 2 * DramSize; //2�� n��
		}
		if (phyMemSizeBits == PAGESIZEBITS) {
			DramSize = 1;
		}
		struct  framePage *Pmemory;
		Pmemory = (struct framePage*)malloc(sizeof(struct framePage)*(DramSize));
		//������ ������ ���� �Ҵ�
		//initPhyMem(Pmemory, DramSize);
		struct procEntry *Ptable;
		Ptable = (struct procEntry*)malloc(sizeof(struct procEntry)*(numProcess));
		//���μ��� �� ���̺� ����
		char** proname = (char**)malloc(sizeof(char*)*numProcess);
		//���μ��� ���� ��ŭ ���μ������� ���� �Ҵ�
		for (i = 0; i < numProcess+1; ++i) {
			proname[i] = (char*)malloc(sizeof(char) * 100);
		}
		for (i = 0; i < numProcess; ++i) {
			proname[i] = argv[i + 5];
		}
		//initProcTable(Ptable, proname,numProcess);
		for (i = 0; i < numProcess; ++i)
			printf("process %d opening %s\n", i, argv[i + 5]);

		int nFrame = (1 << (phyMemSizeBits - PAGESIZEBITS)); assert(nFrame > 0);

		printf("\nNum of Frames %d Physical Memory Size %ld bytes\n", nFrame, (1L << phyMemSizeBits));

		if (atoi(argv[2]) == 0) {
			printf("=============================================================\n");
			printf("The One-Level Page Table with FIFO Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call oneLevelVMSim() with FIFO
			//oneLevelVMSim(struct procEntry *procTable, struct framePage *phyMemFrames, char FIFOorLRU)
			char f = 'F'; char l = 'L';
			initPhyMem(Pmemory, DramSize);
			initProcTable(Ptable, proname, numProcess);
			oneLevelVMSim(Ptable, Pmemory, f, argv);
			for (i = 0; i < numProcess; ++i) {
				free(Ptable[i].firstLevelPageTable);
			}
			
			printf("=============================================================\n");
			printf("The One-Level Page Table with LRU Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			initPhyMem(Pmemory, DramSize);
			initProcTable(Ptable, proname, numProcess);
			oneLevelVMSim(Ptable, Pmemory, l, argv);
			for (i = 0; i < numProcess; ++i) {
				free(Ptable[i].firstLevelPageTable);
			}
		
		}
		else if (atoi(argv[2]) == 1) {
			// initialize procTable for the simulation
			printf("=============================================================\n");
			printf("The Two-Level Page Table Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call twoLevelVMSim()
			initPhyMem(Pmemory, DramSize);
			initProcTable2(Ptable, proname, numProcess);
			twoLevelVMSim(Ptable, Pmemory,argv);

		}
		else if (atoi(argv[2]) == 2) {
			printf("=============================================================\n");
			printf("The Inverted Page Table Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call invertedPageVMsim()
			initPhyMem(Pmemory, DramSize);
			initProcTable(Ptable, proname, numProcess);
			invertedPageVMSim(Ptable, Pmemory, DramSize, argv);
		}
		else if (atoi(argv[2]) == 3) {
			printf("=============================================================\n");
			printf("The One-Level Page Table with FIFO Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call oneLevelVMSim() with FIFO
			//oneLevelVMSim(struct procEntry *procTable, struct framePage *phyMemFrames, char FIFOorLRU)
			char f = 'F'; char l = 'L';
			initPhyMem(Pmemory, DramSize);
			initProcTable(Ptable, proname, numProcess);
			oneLevelVMSim(Ptable, Pmemory, f,argv);
			for (i = 0; i < numProcess; ++i) {
				free(Ptable[i].firstLevelPageTable);
			}
			
			printf("=============================================================\n");
			printf("The One-Level Page Table with LRU Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			initPhyMem(Pmemory, DramSize);
			initProcTable(Ptable, proname, numProcess);
			oneLevelVMSim(Ptable, Pmemory, l,argv);
			for (i = 0; i < numProcess; ++i) {
				free(Ptable[i].firstLevelPageTable);
			}
			
			// initialize procTable for the simulation
			printf("=============================================================\n");
			printf("The Two-Level Page Table Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call twoLevelVMSim()
			initPhyMem(Pmemory, DramSize);
			initProcTable2(Ptable, proname, numProcess);
			twoLevelVMSim(Ptable, Pmemory,argv);
			for(i=0;i<firstLevelBits;++i){
				DramSize2 = DramSize2*2;
			}
			for (i = 0; i < numProcess; ++i) {
				for(j=0;j<DramSize2;++j){
				if (Ptable[i].firstLevelPageTable[j].secondLevelPageTable != NULL) {
					free(Ptable[i].firstLevelPageTable[j].secondLevelPageTable);
					}
				}
				free(Ptable[i].firstLevelPageTable);
			}
			// initialize procTable for the simulation
			printf("=============================================================\n");
			printf("The Inverted Page Table Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call invertedPageVMsim()
			initPhyMem(Pmemory, DramSize);
			initProcTable(Ptable, proname, numProcess);
			invertedPageVMSim(Ptable, Pmemory, DramSize,argv);
		}
		/*
		// initialize procTable for the simulation
		for (i = 0; i < numProcess; i++) {
			// initialize procTable fields
			// rewind tracefiles
			rewind(Ptable[i].tracefp);
		}*/
	}
	else {
		if(argc<5){
                printf("Usage : %s simType firstLevelBits PhysicalMemorySizeBIts TraceFileNames\n",argv[0]);
                exit(1);
                }
	
		firstLevelBits = atoi(argv[2]);
		phyMemSizeBits = atoi(argv[3]);
		if (phyMemSizeBits < PAGESIZEBITS) {
			printf("PhysicalMemorySizeBits %d should be larger than PageSizeBits %d\n", phyMemSizeBits, PAGESIZEBITS); exit(1);
		}
		if (VIRTUALADDRBITS - PAGESIZEBITS - firstLevelBits <= 0) {
			printf("firstLevelBits %d is too Big for the 2nd level page system\n", firstLevelBits); exit(1);
		}
		numProcess = argc - 4;
		for (i = 0; i < phyMemSizeBits - 12; ++i) {
			DramSize = 2 * DramSize; //2�� n��
		}
		if (phyMemSizeBits == PAGESIZEBITS) {
			DramSize = 1;
		}
		struct  framePage *Pmemory;
		Pmemory = (struct framePage*)malloc(sizeof(struct framePage)*(DramSize));
		//������ ������ ���� �Ҵ�
		//initPhyMem(Pmemory, DramSize);
		struct procEntry *Ptable;
		Ptable = (struct procEntry*)malloc(sizeof(struct procEntry)*(numProcess));
		//���μ��� �� ���̺� ����
		char** proname = (char**)malloc(sizeof(char*)*numProcess); // trace name
		for (i = 0; i < numProcess+1; ++i) {
			proname[i] = (char*)malloc(sizeof(char) * 100);
		}
		for (i = 0; i < numProcess; ++i) {
			proname[i] = argv[i + 4];
		}
		//initProcTable(Ptable, proname,numProcess);
		for (i = 0; i < numProcess; ++i)
			printf("process %d opening %s\n", i, argv[i+4]); //���γ������� �ٲ㺸�� �׷��� �ȵ�
		int nFrame = (1 << (phyMemSizeBits - PAGESIZEBITS)); assert(nFrame > 0);

		printf("\nNum of Frames %d Physical Memory Size %ld bytes\n", nFrame, (1L << phyMemSizeBits));
		//argv[1]==s && argv[2] == 0�̸� ����������
	    // else�϶� argv[1] == 0�̸� ����������
		if (atoi(argv[1]) == 0) {
			printf("=============================================================\n");
			printf("The One-Level Page Table with FIFO Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call oneLevelVMSim() with FIFO
			//oneLevelVMSim(struct procEntry *procTable, struct framePage *phyMemFrames, char FIFOorLRU)
			char f = 'F'; char l = 'L';
			initProcTable(Ptable, proname, numProcess);
			initPhyMem(Pmemory, DramSize);
			oneLevelVMSim(Ptable, Pmemory, f, argv);
			for (i = 0; i < numProcess; ++i) {
				free(Ptable[i].firstLevelPageTable);
			}
			printf("=============================================================\n");
			printf("The One-Level Page Table with LRU Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			initPhyMem(Pmemory, DramSize);
			initProcTable(Ptable, proname, numProcess);
			oneLevelVMSim(Ptable, Pmemory, l, argv);

		}
		else if (atoi(argv[1]) == 1) {
			// initialize procTable for the simulation
			printf("=============================================================\n");
			printf("The Two-Level Page Table Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call twoLevelVMSim()
			initPhyMem(Pmemory, DramSize);
			initProcTable2(Ptable, proname, numProcess);
			twoLevelVMSim(Ptable, Pmemory,argv);
		}
		else if (atoi(argv[1]) == 2) {
			printf("=============================================================\n");
			printf("The Inverted Page Table Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call invertedPageVMsim()
			initPhyMem(Pmemory, DramSize);
			initProcTable(Ptable, proname, numProcess);
			invertedPageVMSim(Ptable, Pmemory, DramSize, argv);
		}
		else if (atoi(argv[1]) == 3) { //���δ� �ٵ� �ϴ� �����������̺�����
			printf("=============================================================\n");
			printf("The One-Level Page Table with FIFO Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call oneLevelVMSim() with FIFO
			//oneLevelVMSim(struct procEntry *procTable, struct framePage *phyMemFrames, char FIFOorLRU)
			char f = 'F'; char l = 'L';
			initPhyMem(Pmemory, DramSize);
			initProcTable(Ptable, proname, numProcess);
			oneLevelVMSim(Ptable, Pmemory, f,argv);
			printf("=============================================================\n");
			printf("The One-Level Page Table with LRU Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			for (i = 0; i < numProcess; ++i) {
				free(Ptable[i].firstLevelPageTable);
			}
			
			initPhyMem(Pmemory, DramSize);
			initProcTable(Ptable, proname, numProcess);
			oneLevelVMSim(Ptable, Pmemory, l,argv);
			for (i = 0; i < numProcess; ++i) {
				free(Ptable[i].firstLevelPageTable);
			}
			
			// initialize procTable for the simulation
			printf("=============================================================\n");
			printf("The Two-Level Page Table Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call twoLevelVMSim()
			initPhyMem(Pmemory, DramSize);
			initProcTable2(Ptable, proname, numProcess);
			twoLevelVMSim(Ptable, Pmemory,argv);
			for(i=0;i<firstLevelBits;++i){
				DramSize2 = 2*DramSize2;
			}
			for (i = 0; i < numProcess; ++i) {
				for(j=0;j<DramSize2;++j){
				if (Ptable[i].firstLevelPageTable[j].secondLevelPageTable != NULL) {
					free(Ptable[i].firstLevelPageTable[j].secondLevelPageTable);
				}
				}
				free(Ptable[i].firstLevelPageTable);
			}
			// initialize procTable for the simulation
			printf("=============================================================\n");
			printf("The Inverted Page Table Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call invertedPageVMsim()
			initPhyMem(Pmemory, DramSize);
			initProcTable(Ptable, proname, numProcess);
			invertedPageVMSim(Ptable, Pmemory, DramSize,argv);
		}

		// initialize procTable for the simulation
		/*
		for (i = 0; i < numProcess; i++) {
			// initialize procTable fields
			// rewind tracefiles
			rewind(Ptable[i].tracefp);
		}*/
	}

	return(0);
}

