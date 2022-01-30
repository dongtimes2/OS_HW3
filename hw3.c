//
// Virual Memory Simulator Homework
// One-level page table system with FIFO and LRU
// Two-level page table system with LRU
// Inverted page table with a hashing system
// Submission Year: 2021
// Student Name: 유동하
// Student Number: B731165
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#define PAGESIZEBITS 12			// page size = 4Kbytes
#define VIRTUALADDRBITS 32		// virtual address space size = 4Gbytes

int numProcess, simType, firstLevelBits;
unsigned int nFrame;

struct procEntry {
	char *traceName;			// the memory trace name
	int pid;					// process (trace) id
	int ntraces;				// the number of memory traces
	int num2ndLevelPageTable;	// The 2nd level page created(allocated);
	int numIHTConflictAccess; 	// The number of Inverted Hash Table Conflict Accesses 노드 접근 수
	int numIHTNULLAccess;		// The number of Empty Inverted Hash Table Accesses
	int numIHTNonNULLAcess;		// The number of Non Empty Inverted Hash Table Accesses
	int numPageFault;			// The number of page faults
	int numPageHit;				// The number of page hits
	struct pageTableEntry *firstLevelPageTable;	//이 프로세스의 페이지테이블을 연결시켜주기 위함
	FILE *tracefp;				// 프로세스에 해당하는 trace파일의  계속 엑세스하기 위해서.
} *procTable;

struct hashTableEntry {
	int procNum;
	int idx;
	int pIdx;
	struct hashTableEntry *node;
	struct hashTableEntry *next;
	struct hashTableEntry *prev;
} *hashTable;

struct pageTableEntry {
	//page table에 올라온 순서도 고려해야 한다.
	int procNum;
	int valid;
	int idx;	//그냥 or 퍼스트 페이지 인덱스
	int sIdx;	//쌔컨 페이지의 인덱스
	int pIdx;	//피지컬 프레임 인덱스
	int secondFlag;
	struct pageTableEntry * page;
	struct pageTableEntry * next;
	struct pageTableEntry * prev;
} pageTableQueue, **pageTable, *secondpagetable;

struct simpleTableEntry{
	int procNum;
	int idx;
	int pIdx;
	int count;
	struct simpleTableEntry *next;
	struct simpleTableEntry *prev;
} *simpleTable;

struct pageTableEntry *front = &pageTableQueue;
struct pageTableEntry *rear = &pageTableQueue;

void initProcTable(){
	int i;
	for(i=0; i<numProcess; i++){
		procTable[i].traceName = 0;			// the memory trace name
		procTable[i].pid = i;					// process (trace) id
		procTable[i].ntraces = 0;				// the number of memory traces
		procTable[i].num2ndLevelPageTable = 0;	// The 2nd level page created(allocated);
		procTable[i].numIHTConflictAccess = 0; 	// The number of Inverted Hash Table Conflict Accesses
		procTable[i].numIHTNULLAccess = 0;		// The number of Empty Inverted Hash Table Accesses
		procTable[i].numIHTNonNULLAcess = 0;		// The number of Non Empty Inverted Hash Table Accesses
		procTable[i].numPageFault = 0;			// The number of page faults
		procTable[i].numPageHit = 0;				// The number of page hits
		procTable[i].firstLevelPageTable = NULL;
		procTable[i].tracefp = NULL;
	}
}


unsigned int power (int a, int b){
	unsigned int result = 1, i;
	for(i = 0; i < b; i++) result *= 2;
	return result;
}


void oneLevelVMSim() {
	//이곳에서 FIFO와 LRU를 동시에 처리해줘야 한다.

	int i, idx;
	unsigned addr;
	char rw;
	int flag = 0, frameEnoughFlag = 1;
	int memoryPtr = 0;

	pageTable = (struct pageTableEntry **) malloc(sizeof(struct pageTableEntry*) * numProcess);
	for(i=0; i<numProcess; i++){
		pageTable[i] = (struct pageTableEntry *) malloc(sizeof(struct pageTableEntry) * power(2, 20));
	}

	while(1){
		for(i=0; i < numProcess; i++) {
			if(EOF != fscanf(procTable[i].tracefp, "%x %c", &addr, &rw)){
				//printf("디버깅 / 프로세스 : %d, 주소 : %x, ", i, addr);
				//printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
				//FIFO 알고리즘 일 때
				if(simType == 0){
					//가상주소 앞 5글자를 인덱스로 하여 페이지 테이블에 접근한다.
					idx = (addr>>12);
					//printf("인덱스 : %d\n", idx);

					if(pageTable[i][idx].valid){
						//printf("힛\n");
						procTable[i].numPageHit++;
						procTable[i].ntraces++;
					}

					else{
						//printf("미스\n");
						struct pageTableEntry *temp = NULL;

						//아직 프레임에 남는 자리가 있을 때 (여기서 링크드 리스트로 적재된 순서를 구현해줘야 한다.)
						if(memoryPtr < nFrame) {
							procTable[i].numPageFault++;
							procTable[i].ntraces++;
							pageTable[i][idx].valid = 1;
							pageTable[i][idx].idx = idx;
							pageTable[i][idx].procNum = i;
							pageTable[i][idx].pIdx = memoryPtr;

							memoryPtr++;

							temp = rear;

							rear->next = &pageTable[i][idx];
							rear = &pageTable[i][idx];
							rear->next = NULL;
							rear->prev = temp;
							temp = NULL;
							//printf("남는자리가 있어 인덱스 %d를 유효화 함\n", idx);
						}

						//프레임이 꽉 찼을 때
						else {
							int delIdx = front->next->idx;
							int delProc = front->next->procNum;

							pageTable[delProc][delIdx].valid = 0;
							//printf("무효화할 프로세스 : %d, 인덱스 : %d\n", delProc, delIdx);

							if(front->next == rear){
								front->next->prev = NULL;
								front->next = NULL;
								rear = &pageTableQueue;
							}

							else{
								temp = front->next->next;
								temp->prev = front;
								front->next->next = NULL;
								front->next->prev = NULL;
								front->next = temp;
								temp=NULL;
							}

							//기존의 것 내보내기 완료한 다음, 새로 들어올 것을 1로 세팅해주고 큐에 삽입
							procTable[i].numPageFault++;
							procTable[i].ntraces++;
							pageTable[i][idx].valid = 1;
							pageTable[i][idx].idx = idx;
							pageTable[i][idx].procNum = i;

							temp = rear;

							rear->next = &pageTable[i][idx];
							rear = &pageTable[i][idx];
							rear->next = NULL;
							rear->prev = temp;
							temp = NULL;
						}
					}
				}

				//LRU 알고리즘일 때
				//힛 할때마다 링크드 리스트를 옮기면 될 것 같다.

				else{
					//가상주소 앞 5글자를 인덱스로 하여 페이지 테이블에 접근한다.
					idx = (addr>>12);
					struct pageTableEntry *temp = NULL;
					//printf("인덱스 : %d\n", idx);

					if(pageTable[i][idx].valid){
						//printf("힛\n");
						procTable[i].numPageHit++;
						procTable[i].ntraces++;

						//페이지 테이블의 각 엔트리에 prev, next 포인터 정보 직접 저장
						//linked list 탐색을 이용하는 것보다 빠름

						//맨 뒤에 위치한 경우
						if(pageTable[i][idx].next == NULL){}

						//맨 앞에 위치한 경우
						else if(pageTable[i][idx].prev == front){
							pageTable[i][idx].prev->next = pageTable[i][idx].next;
							pageTable[i][idx].next->prev = front;

							pageTable[i][idx].next=NULL;

							rear->next = &pageTable[i][idx];
							pageTable[i][idx].prev = rear;
							rear=&pageTable[i][idx];
						}

						//중간에 위치한 경우
						else{
							pageTable[i][idx].prev->next = pageTable[i][idx].next;
							pageTable[i][idx].next->prev = pageTable[i][idx].prev;

							pageTable[i][idx].next=NULL;

							rear->next = &pageTable[i][idx];
							pageTable[i][idx].prev = rear;
							rear=&pageTable[i][idx];
						}
					}

					else{
						//printf("미스\n");

						//아직 프레임에 남는 자리가 있을 때 (여기서 링크드 리스트로 적재된 순서를 구현해줘야 한다.)
						if(memoryPtr < nFrame) {
							procTable[i].numPageFault++;
							procTable[i].ntraces++;
							pageTable[i][idx].valid = 1;
							pageTable[i][idx].idx = idx;
							pageTable[i][idx].procNum = i;
							pageTable[i][idx].pIdx = memoryPtr;

							memoryPtr++;

							temp = rear;

							rear->next = &pageTable[i][idx];
							rear = &pageTable[i][idx];
							rear->next = NULL;
							rear->prev = temp;
							temp = NULL;

							//새로 추가
							pageTable[i][idx].next = NULL;
							pageTable[i][idx].prev = rear->prev;

							//printf("남는자리가 있어 인덱스 %d를 유효화 함\n", idx);
						}

						//프레임이 꽉 찼을 때
						else {
							int delIdx = front->next->idx;
							int delProc = front->next->procNum;
							int frontNextEqRearFlag = 0;
							pageTable[delProc][delIdx].valid = 0;
							//printf("무효화할 프로세스 : %d, 인덱스 : %d\n", delProc, delIdx);

							if(front->next == rear){
								front->next->prev = NULL;
								front->next = NULL;
								rear = &pageTableQueue;

								//새로추가
								pageTable[delProc][delIdx].next = NULL;
								pageTable[delProc][delIdx].prev = NULL;
								frontNextEqRearFlag=1;
							}

							else{
								temp = front->next->next;
								temp->prev = front;
								front->next->next = NULL;
								front->next->prev = NULL;
								front->next = temp;
								temp=NULL;

								//새로추가
								pageTable[delProc][delIdx].next = NULL;
								pageTable[delProc][delIdx].prev = NULL;
							}

							//기존의 것 내보내기 완료한 다음, 새로 들어올 것을 1로 세팅해주고 큐에 삽입
							procTable[i].numPageFault++;
							procTable[i].ntraces++;
							pageTable[i][idx].valid = 1;
							pageTable[i][idx].idx = idx;
							pageTable[i][idx].procNum = i;

							temp = rear;

							//기존 노드 수정
							int preExistIdx = rear->idx;
							int preExistProc = rear->procNum;

							rear->next = &pageTable[i][idx];
							rear = &pageTable[i][idx];
							rear->next = NULL;
							rear->prev = temp;
							temp = NULL;

							//새로 추가할 노드
							if(!frontNextEqRearFlag){
								pageTable[preExistProc][preExistIdx].next = rear;
							}

							pageTable[i][idx].next = NULL;
							pageTable[i][idx].prev = rear->prev;
						}
					}
				}
			}

			else {
				flag = 1;
				break;
			}
		}
		if(flag) break;
	}

	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
	}
}

void twoLevelVMSim() {
	//이것 역시 프로세스 당 한 개의 pagetable을 만들 수 있도록 짜야한다.

	int i;
	unsigned addr, firstIdx, secondIdx, idx;
	char rw;
	int flag = 0, frameEnoughFlag = 1, backBitCnt;
	int memoryPtr = 0;

	//일단 첫 번째 비트 수만큼의 인덱스를 가진 페이지 테이블 생성
	pageTable = (struct pageTableEntry **) malloc(sizeof(struct pageTableEntry*) * numProcess);
	for(i=0; i<numProcess; i++){
		pageTable[i] = (struct pageTableEntry *) malloc(sizeof(struct pageTableEntry) * power(2, firstLevelBits));
	}

	while(1){
		for(i=0; i < numProcess; i++) {
			if(EOF != fscanf(procTable[i].tracefp, "%x %c", &addr, &rw)){
				//printf("디버깅 / 프로세스 : %d, 주소 : %x, ", i, addr);

				//입력받은 first bit만큼 주소를 잘라야 한다.
				backBitCnt = 20-firstLevelBits;
				idx = (addr>>12);
				firstIdx = (idx>>(20-firstLevelBits));
				secondIdx = (idx<<(32-backBitCnt));
				secondIdx = (secondIdx>>(32-backBitCnt));
				//printf("인덱스 : %d, 퍼스트 인덱스 : %d, 세컨드 인덱스 %d\n", idx, firstIdx, secondIdx);

				struct pageTableEntry *temp = NULL;

				//힛
				if(pageTable[i][firstIdx].secondFlag && pageTable[i][firstIdx].page[secondIdx].valid){
					//printf("힛\n");
					procTable[i].numPageHit++;
					procTable[i].ntraces++;

					//맨 뒤에 있는 경우
					if(pageTable[i][firstIdx].page[secondIdx].next == NULL){}

					//맨 앞에 있는 경우
					else if(pageTable[i][firstIdx].page[secondIdx].prev == front){
						pageTable[i][firstIdx].page[secondIdx].prev->next = pageTable[i][firstIdx].page[secondIdx].next;
						pageTable[i][firstIdx].page[secondIdx].next->prev = front;

						pageTable[i][firstIdx].page[secondIdx].next = NULL;

						rear->next=&pageTable[i][firstIdx].page[secondIdx];
						pageTable[i][firstIdx].page[secondIdx].prev=rear;
						rear=&pageTable[i][firstIdx].page[secondIdx];
					}

					//중간에 있는 경우
					else{
						pageTable[i][firstIdx].page[secondIdx].prev->next = pageTable[i][firstIdx].page[secondIdx].next;
						pageTable[i][firstIdx].page[secondIdx].next->prev = pageTable[i][firstIdx].page[secondIdx].prev;

						pageTable[i][firstIdx].page[secondIdx].next = NULL;

						rear->next=&pageTable[i][firstIdx].page[secondIdx];
						pageTable[i][firstIdx].page[secondIdx].prev=rear;
						rear=&pageTable[i][firstIdx].page[secondIdx];
					}
				}

				//미스
				else{
					//테이블이 없는 미스인 경우
					if(!pageTable[i][firstIdx].secondFlag){
						//printf("아직 세컨 테이블 없음\n");

						secondpagetable = (struct pageTableEntry *) malloc(sizeof(struct pageTableEntry) * power(2, backBitCnt));
						pageTable[i][firstIdx].secondFlag = 1;
						pageTable[i][firstIdx].page = secondpagetable;

						//아직 프레임에 남는 자리가 있을 때
						if(memoryPtr < nFrame) {
							procTable[i].numPageFault++;
							procTable[i].ntraces++;
							procTable[i].num2ndLevelPageTable++;

							pageTable[i][firstIdx].page[secondIdx].valid = 1;
							pageTable[i][firstIdx].page[secondIdx].idx = firstIdx;
							pageTable[i][firstIdx].page[secondIdx].sIdx = secondIdx;
							pageTable[i][firstIdx].page[secondIdx].procNum = i;
							pageTable[i][firstIdx].page[secondIdx].pIdx = memoryPtr;

							memoryPtr++;

							temp = rear;

							rear->next = &pageTable[i][firstIdx].page[secondIdx];
							rear = &pageTable[i][firstIdx].page[secondIdx];
							rear->next = NULL;
							rear->prev = temp;
							temp = NULL;
							//printf("남는자리가 있어 인덱스 %d를 유효화 함\n", idx);

							//새로 추가
							pageTable[i][firstIdx].page[secondIdx].next=NULL;
							pageTable[i][firstIdx].page[secondIdx].prev = rear->prev;
						}

						//프레임이 꽉 찼을 때, 기존 것을 빼야 함
						else{
							int delFirstIdx = front->next->idx;
							int delSecondIdx = front->next->sIdx;
							int delProc = front->next->procNum;
							int frontNextEqRearFlag = 0;

							pageTable[delProc][delFirstIdx].page[delSecondIdx].valid = 0;
							//printf("무효화할 프로세스 : %d, 앞 인덱스 : %d, 뒷 인덱스 :%d\n", delProc, delFirstIdx, delSecondIdx);

							if(front->next == rear){
								//printf("앞에 위치\n");
								front->next->prev = NULL;
								front->next = NULL;
								rear = &pageTableQueue;

								//새로 추가
								pageTable[delProc][delFirstIdx].page[delSecondIdx].next = NULL;
								pageTable[delProc][delFirstIdx].page[delSecondIdx].prev = NULL;
								frontNextEqRearFlag=1;
							}

							else{
								//printf("중간이나 뒤에 위치\n");
								temp = front->next->next;
								temp->prev = front;
								front->next->next = NULL;
								front->next->prev = NULL;
								front->next = temp;
								temp=NULL;

								//새로 추가
								pageTable[delProc][delFirstIdx].page[delSecondIdx].next=NULL;
								pageTable[delProc][delFirstIdx].page[delSecondIdx].prev=NULL;
							}

							//기존의 것 내보내기 완료한 다음, 새로 들어올 것을 1로 세팅해주고 큐에 삽입
							procTable[i].numPageFault++;
							procTable[i].ntraces++;
							procTable[i].num2ndLevelPageTable++;

							pageTable[i][firstIdx].page[secondIdx].valid = 1;
							pageTable[i][firstIdx].page[secondIdx].idx = firstIdx;
							pageTable[i][firstIdx].page[secondIdx].sIdx = secondIdx;
							pageTable[i][firstIdx].page[secondIdx].procNum = i;

							temp = rear;

							int preExistFirstIdx = rear->idx;
							int preExistSecondIdx = rear->sIdx;
							int preExistProc = rear->procNum;
							//printf("기존 리어 프로세스번호 : %d, 앞 인덱스 : %d, 뒤 인덱스 : %d\n", preExistProc, preExistFirstIdx, preExistSecondIdx);

							rear->next = &pageTable[i][firstIdx].page[secondIdx];
							rear = &pageTable[i][firstIdx].page[secondIdx];
							rear->next = NULL;
							rear->prev = temp;
							temp = NULL;

							//새로 추가
							if(!frontNextEqRearFlag){
								pageTable[preExistProc][preExistFirstIdx].page[preExistSecondIdx].next=rear;
							}

							pageTable[i][firstIdx].page[secondIdx].next = NULL;
							pageTable[i][firstIdx].page[secondIdx].prev = rear->prev;
							//printf("테이블이 없을 때 빼기작업 끝\n");
						}
					}

					//테이블이 있는 미스인 경우
					else{
						//아직 프레임에 남는 자리가 있을 때 (여기서 링크드 리스트로 적재된 순서를 구현해줘야 한다.)
						if(memoryPtr < nFrame) {
							procTable[i].numPageFault++;
							procTable[i].ntraces++;

							pageTable[i][firstIdx].page[secondIdx].valid = 1;
							pageTable[i][firstIdx].page[secondIdx].idx = firstIdx;
							pageTable[i][firstIdx].page[secondIdx].sIdx = secondIdx;
							pageTable[i][firstIdx].page[secondIdx].procNum = i;
							pageTable[i][firstIdx].page[secondIdx].pIdx = memoryPtr;

							memoryPtr++;

							temp = rear;

							rear->next = &pageTable[i][firstIdx].page[secondIdx];
							rear = &pageTable[i][firstIdx].page[secondIdx];
							rear->next = NULL;
							rear->prev = temp;
							temp = NULL;
							//printf("남는자리가 있어 인덱스 %d를 유효화 함\n", idx);

							//새로 추가
							pageTable[i][firstIdx].page[secondIdx].next=NULL;
							pageTable[i][firstIdx].page[secondIdx].prev = rear->prev;
						}

						//프레임이 꽉 찼을 때
						else {
							int delFirstIdx = front->next->idx;
							int delSecondIdx = front->next->sIdx;
							int delProc = front->next->procNum;
							int frontNextEqRearFlag = 0;

							pageTable[delProc][delFirstIdx].page[delSecondIdx].valid = 0;
							//printf("무효화할 프로세스 : %d, 인덱스 : %d\n", delProc, delIdx);

							if(front->next == rear){
								front->next->prev = NULL;
								front->next = NULL;
								rear = &pageTableQueue;

								//새로 추가
								pageTable[delProc][delFirstIdx].page[delSecondIdx].next = NULL;
								pageTable[delProc][delFirstIdx].page[delSecondIdx].prev = NULL;
								frontNextEqRearFlag=1;
							}

							else{
								temp = front->next->next;
								temp->prev = front;
								front->next->next = NULL;
								front->next->prev = NULL;
								front->next = temp;
								temp=NULL;

								//새로 추가
								pageTable[delProc][delFirstIdx].page[delSecondIdx].next = NULL;
								pageTable[delProc][delFirstIdx].page[delSecondIdx].prev = NULL;
							}

							//기존의 것 내보내기 완료한 다음, 새로 들어올 것을 1로 세팅해주고 큐에 삽입
							procTable[i].numPageFault++;
							procTable[i].ntraces++;

							pageTable[i][firstIdx].page[secondIdx].valid = 1;
							pageTable[i][firstIdx].page[secondIdx].idx = firstIdx;
							pageTable[i][firstIdx].page[secondIdx].sIdx = secondIdx;
							pageTable[i][firstIdx].page[secondIdx].procNum = i;

							temp = rear;

							int preExistFirstIdx = rear->idx;
							int preExistSecondIdx = rear->sIdx;
							int preExistProc = rear->procNum;

							rear->next = &pageTable[i][firstIdx].page[secondIdx];
							rear = &pageTable[i][firstIdx].page[secondIdx];
							rear->next = NULL;
							rear->prev = temp;
							temp = NULL;

							//새로 추가
							if(!frontNextEqRearFlag){
								pageTable[preExistProc][preExistFirstIdx].page[preExistSecondIdx].next=rear;
							}

							pageTable[i][firstIdx].page[secondIdx].next = NULL;
							pageTable[i][firstIdx].page[secondIdx].prev = rear->prev;
						}
					}
				}
			}

			else {
				flag = 1;
				break;
			}
		}
		if(flag) break;
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


void invertedPageVMSim() {
	int i, idx, hashIdx, frameIdx;
	unsigned addr;
	char rw;
	int flag = 0, frameEnoughFlag = 1;
	int memoryPtr = 0;
	//int cnt=1;
	int currentCnt=1;


	//도저히 링크드리스트로 못하겠어서 counter로 방식을 변경함

	while(1){
		for(i=0; i < numProcess; i++) {
			if(EOF != fscanf(procTable[i].tracefp, "%x %c", &addr, &rw)){
				//printf("%d\n", cnt);

				idx = (addr>>12);
				hashIdx = (idx+i) % nFrame;
				//printf("디버깅 / 프로세스 : %d, 주소 : %x, 인덱스 : %d, 해시 인덱스 : %d\n", i, addr, idx, hashIdx);


				//매핑이 한 개라도 존재하는 경우
				if(hashTable[hashIdx].node != NULL){
					//printf("매핑이 존재함\n");

					procTable[i].ntraces++;
					procTable[i].numIHTNonNULLAcess++;

					//테이블에 붙일 신규 노드 생성
					struct hashTableEntry *tempNode = malloc(sizeof(struct hashTableEntry));
					tempNode->procNum = i;
					tempNode->idx = idx;

					struct hashTableEntry *iter = hashTable[hashIdx].node;
					//printf("기존 해시테이블 바로 옆 인덱스 값 : %d, 프로세스 값 : %d\n", hashTable[hashIdx].node->idx, hashTable[hashIdx].node->procNum);

					int hitFlag = 0;
					int hitFrameIdx;

					while(iter!=NULL){
						//printf("매칭 반복문 진입\n");
						//printf("iter 인덱스 : %d, iter 프로세스 : %d\n", iter->idx, iter->procNum);

						//발견한 경우
						if(iter->idx == idx && iter->procNum == i){
							hitFlag = 1;
							//printf("힛 플래그 달성\n");
							procTable[i].numIHTConflictAccess++;
							hitFrameIdx = iter->pIdx;
							procTable[i].numPageHit++;
							simpleTable[hitFrameIdx].count = currentCnt++;
							break;
						}

						//발견하지 못한 경우
						else{
							//printf("발견 못함\n");
							procTable[i].numIHTConflictAccess++;
							iter = iter->next;
						}
					}

					iter=NULL;

					//찾는 엔트리가 없는 경우
					if(!hitFlag){
						//printf("미스\n");
						procTable[i].numPageFault++;

						//프레임에 남는 자리가 있을 때
						if(memoryPtr < nFrame){
							//printf("프레임에 남는 자리 있음\n");

							tempNode->pIdx = memoryPtr;

							//기존 매핑 앞에다가 삽입해야 함
							tempNode->next = hashTable[hashIdx].node;
							hashTable[hashIdx].node = tempNode;

							//printf("디버깅 / %d %d\n", hashTable[hashIdx].node->idx, hashTable[hashIdx].node->next->idx);

							//큐에서 새로 들어온 노드 삽입
							simpleTable[memoryPtr].count = currentCnt++;
							simpleTable[memoryPtr].idx = idx;
							simpleTable[memoryPtr].procNum = i;
							simpleTable[memoryPtr].pIdx = memoryPtr;

							//printf("디버깅2 %d, %d\n", hashTable[hashIdx].node->idx, hashTable[hashIdx].node->next->idx);
							memoryPtr++;
						}

						//프레임이 꽉 찼을 때, 기존 것을 빼야 함
						else{
							//printf("프레임에 남는 자리 없음\n");
							//가장 작은 count 값을 가진 index 탐색

							int j, minJ, minIdx, minProc, minCnt = INT_MAX;
							for(j=0; j<nFrame; j++){
								if(minCnt>simpleTable[j].count){
									minCnt = simpleTable[j].count;
									minIdx = simpleTable[j].idx;
									minProc = simpleTable[j].procNum;
									minJ = j;
								}
							}

							// for(j=0; j<nFrame; j++){
							// 	printf("%d번 프레임의 카운트 : %d\n", j, simpleTable[j].count);
							// }

							int delIdx = minIdx;
							int delProc = minProc;
							int delHashIdx = (delIdx + delProc) % nFrame;

							//삭제할 노드가 들어있는 해당 인덱스로 들어가서 해당 노드를 삭제한다.
							struct hashTableEntry *delNode = malloc(sizeof(struct hashTableEntry));		//삭제해야 될 노드의 정보
							struct hashTableEntry *iter2 = hashTable[delHashIdx].node;					//순환할 노드
							struct hashTableEntry *prevIter = NULL;										//순환할 노드의 이전노드

							//printf("삭제할 인덱스 : %d, 삭제할 프로세스 : %d, 삭제할 것의 카운트 : %d\n", delIdx, delProc, minCnt);

							delNode->procNum = delProc;
							delNode->idx = delIdx;

							while(1) {
 								//printf("이터 인덱스 : %d, 이터 프로세스 : %d\n", iter2->idx, iter2->procNum);

								if((iter2->idx == delNode->idx) && (iter2->procNum == delNode->procNum)){

									//삭제할 노드가 맨 앞에 있는 경우
									if(hashTable[delHashIdx].node == iter2){
										//printf("삭제 노드 맨 앞\n");

										//노드가 한 개만 달린 경우
										if(iter2->next == NULL){
											//printf("노드가 한 개여서 null 처리\n");
											hashTable[delHashIdx].node = NULL;
										}

										//노드가 여러 개 달린 경우
										else{
											//printf("노드가 한 개가 아님\n");
											hashTable[delHashIdx].node = iter2->next;
										}
									}

									//삭제할 노드가 중간이나 맨 뒤에 있는 경우
									else{
										//printf("삭제 노드 맨 앞 아님\n");
										prevIter->next = iter2->next;
									}
									break;
								}

								//발견하지 못한 경우
								else {
									//printf("발견 못함\n");
									prevIter = iter2;
									iter2 = iter2->next;
								}
							}

							iter2 = NULL;
							prevIter = NULL;
							free(delNode);

							//비로소 새로 들어온 값을 추가한다. 기존 값 앞에다가 삽입
							tempNode->pIdx = minJ;
							tempNode->next = hashTable[hashIdx].node;
							hashTable[hashIdx].node = tempNode;

							//printf("띠 %d\n", hashTable[hashIdx].node->idx);
							simpleTable[minJ].count = currentCnt++;
							simpleTable[minJ].idx = idx;
							simpleTable[minJ].procNum = i;
						}
					}
				}


				//해시테이블에 접근했는데 매핑이 한 개도 없는 경우
				else{
					//printf("매핑 없음\n");
					//printf("미스\n");
					procTable[i].numPageFault++;
					procTable[i].ntraces++;
					procTable[i].numIHTNULLAccess++;

					//테이블에 붙일 신규 노드 생성
					struct hashTableEntry *tempNode = malloc(sizeof(struct hashTableEntry));
					tempNode->procNum = i;
					tempNode->idx = idx;

					//프레임에 남는 자리가 있을 때
					if(memoryPtr < nFrame){
						//printf("프레임에 남는 자리 있음\n");

						tempNode->pIdx = memoryPtr;

						//매핑이 한 개도 없기 떄문에 해시테이블에 바로 붙이고 끝
						hashTable[hashIdx].node = tempNode;

						//printf("인덱스 : %d, 노드의 인덱스 : %d\n", idx, hashTable[hashIdx].node->idx);

						//counter 기법을 통해 현재 페이지에 대한 정보 저장
						simpleTable[memoryPtr].count = currentCnt++;
						simpleTable[memoryPtr].idx = idx;
						simpleTable[memoryPtr].procNum = i;
						simpleTable[memoryPtr].pIdx = memoryPtr;
						memoryPtr++;
					}

					//프레임이 꽉 찼을 때, 기존 것을 빼야 함
					else{
						//printf("프레임에 남는 자리 없음\n");

						//가장 작은 count 값을 가진 index 탐색
						int j, minJ, minIdx, minProc, minCnt = INT_MAX;
						for(j=0; j<nFrame; j++){
							if(minCnt>simpleTable[j].count){
								minCnt = simpleTable[j].count;
								minIdx = simpleTable[j].idx;
								minProc = simpleTable[j].procNum;
								minJ = j;
							}
						}

						int delIdx = minIdx;
						int delProc = minProc;
						int delHashIdx = (delIdx + delProc) % nFrame;


						//삭제할 노드가 들어있는 해당 인덱스로 들어가서 해당 노드를 삭제한다.
						struct hashTableEntry *delNode = malloc(sizeof(struct hashTableEntry));			//삭제해야 될 노드의 정보
						struct hashTableEntry *iter = hashTable[delHashIdx].node;						//순환할 노드
						struct hashTableEntry *prevIter = NULL;											//순환할 노드의 이전노드

						delNode->idx = delIdx;
						delNode->procNum = delProc;

						while(1) {

							//삭제해야 될 노드를 발견한 경우
							if((iter->idx == delNode->idx) && (iter->procNum == delNode->procNum)){

								//삭제할 노드가 맨 앞에 있는 경우
								if(hashTable[delHashIdx].node == iter){

									//노드가 한 개만 달린 경우
									if(iter->next == NULL){
										hashTable[delHashIdx].node = NULL;
									}

									//노드가 여러 개 달린 경우
									else{
										hashTable[delHashIdx].node = iter->next;
									}
								}

								//삭제할 노드가 중간이나 맨 뒤에 있는 경우
								else{
									prevIter->next = iter->next;
								}
								break;
							}

							//발견하지 못한 경우
							else {
								prevIter = iter;
								iter = iter->next;
							}
						}
						iter = NULL;
						prevIter = NULL;
						free(delNode);

						//비로소 새로 들어온 값을 추가한다. 매핑 없으므로 그대로 추가
						tempNode->pIdx = minJ;
						hashTable[hashIdx].node = tempNode;

						simpleTable[minJ].count = currentCnt++;
						simpleTable[minJ].idx = idx;
						simpleTable[minJ].procNum = i;
					}
				}

				//printf("마지막 해시테이블 바로 옆 인덱스 : %d, 프로세스 : %d, 프레임 인덱스 : %d\n", hashTable[hashIdx].node->idx, hashTable[hashIdx].node->procNum, hashTable[hashIdx].node->pIdx);
				//printf("\n\n");
				//cnt++;
			}

			else {
				flag = 1;
				break;
			}
		}
		if(flag) break;
	}

	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of Inverted Hash Table Access Conflicts %d\n",i,procTable[i].numIHTConflictAccess);
		printf("Proc %d Num of Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNULLAccess);
		printf("Proc %d Num of Non-Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNonNULLAcess);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
		assert(procTable[i].numIHTNULLAccess + procTable[i].numIHTNonNULLAcess == procTable[i].ntraces);
	}
}


int main(int argc, char *argv[]) {
	int i, phyMemSizeBits, optind = 1;

	simType = atoi(argv[1]);
	firstLevelBits = atoi(argv[2]);
	phyMemSizeBits = atoi(argv[3]);

	//firstLevelBits가 20이상일 경우 에러메시지 송출
	if(firstLevelBits>=20){
		printf("firstLevelBits %d is too Big for the 2nd level page system\n", firstLevelBits);
		exit(1);
	}

	//phyMemSizeBits가 12미만일 경우 에러메시지 송출
	if(phyMemSizeBits<12){
		printf("PhysicalMemorySizeBits %d should be larger than PageSizeBits 12\n", phyMemSizeBits);
		exit(1);
	}

	nFrame = power(2, phyMemSizeBits-PAGESIZEBITS);
	numProcess = argc - 4;

	procTable = (struct procEntry *) malloc(sizeof(struct procEntry) * numProcess);
	hashTable = (struct hashTableEntry *) malloc(sizeof(struct hashTableEntry) * nFrame);
	simpleTable = (struct simpleTableEntry *) malloc(sizeof(struct simpleTableEntry) * nFrame);

	pageTableQueue.next = pageTableQueue.prev = &pageTableQueue;

	initProcTable();

	// initialize procTable for Memory Simulations
	for(i = 0; i < numProcess; i++) {
		// opening a tracefile for the process
		printf("process %d opening %s\n",i,argv[i + optind + 3]);
		procTable[i].tracefp = fopen(argv[i + optind + 3],"r");
		if (procTable[i].tracefp == NULL) {
			printf("ERROR: can't open %s file; exiting...",argv[i+optind+3]);
			exit(1);
		}
		procTable[i].traceName = argv[i+optind+3];
	}


	printf("Num of Frames %d Physical Memory Size %ld bytes\n",nFrame, (1L<<phyMemSizeBits));

	// for(i = 0; i < numProcess; i++){
	// 	printf("디버깅/ %d 번째 name : %s\n", i, procTable[i].traceName);
	// }

	if (simType == 0) {
		printf("=============================================================\n");
		printf("The One-Level Page Table with FIFO Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		oneLevelVMSim();
	}

	if (simType == 1) {
		printf("=============================================================\n");
		printf("The One-Level Page Table with LRU Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		oneLevelVMSim();
	}

	if (simType == 2) {
		printf("=============================================================\n");
		printf("The Two-Level Page Table Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		twoLevelVMSim();
	}

	if (simType == 3) {
		printf("=============================================================\n");
		printf("The Inverted Page Table Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		invertedPageVMSim();
	}

	//파일 입출력 마감
	for(i = 0; i < numProcess; i++) {
		fclose(procTable[i].tracefp);
	}

	return(0);
}
