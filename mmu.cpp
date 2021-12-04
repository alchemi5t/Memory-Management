#include <iostream>
#include <fstream>
#include <string>
#include <sstream> 
#include <unordered_set>
#include<set>
#include <map>
#include<algorithm>
#include <iterator>
#include<tuple> 
#include<vector>
#include<math.h>
#include<string.h>
#include <deque>
#include <unistd.h> 
using namespace std;
#define MAX_VPAGES 64
#define TAU 49
int MAX_FRAMES;
string process_line;
ifstream processes;
string token;
int HAND = 0;
int instr_ct = 0;
vector<int> randvals;
ifstream randf;
int ofs = 0;
int clock_size;
int prev_reset = 0;
int p_HAND = 0;
bool ohno = false, print_pt = false, print_ft = false, print_summary = false;
int myrandom() { 
if(ofs==randvals.size())
	{ofs = 0;}

	return (randvals[ofs++] % MAX_FRAMES);
 }


class pte_t
{
	public:             
    unsigned int valid:1;
    unsigned int referenced:1; 
    unsigned int modified:1;
    unsigned int write_protect:1;
    unsigned int paged_out:1;
    unsigned int f_no:7;
    unsigned int file_mapped:1;
    pte_t()
    {
valid = 0;
referenced = 0;
modified = 0;
write_protect = 0;
paged_out = 0;
f_no = 0;
file_mapped = 0;
    }
    
};

class frame_t
{
	public:             
    int proc_id;
    int vpage_id; 
    int frame_id;
    bool used;
    unsigned int age; // todo: might need something larger than int, idk, check later 
    int timestamp;
    frame_t(){}
    frame_t(int a)
    {
    	frame_id = a;
    	used = false;
    }
    
};


frame_t * frame_table;
deque<frame_t*> free_pool;

class vma{
	public:             
    int start;
    int end; 
    unsigned int write_protected:1;
    unsigned int file_mapped:1;
    vma(int a, int b,int c, int d)
    {
    	start = a;
    	end = b;
    	write_protected = c;
    	file_mapped = d;
  
    }

};
class globalstats{
public:
	int unsigned long long g_ins;
	int unsigned long long cs;
	int unsigned long long pes;
	int unsigned long long cost;
	globalstats(){
g_ins = 0;
cs = 0;
pes = 0;
cost = 0;


	}


};
globalstats * g;

class process {       
  public:             
    vector<vma> vmas;
    pte_t page_table[MAX_VPAGES];  
    int proc_id;
    int unsigned long unmaps;
    int unsigned long maps;
    int unsigned long ins;
    int unsigned long outs;
    int unsigned long fins;
    int unsigned long fouts;
    int unsigned long zeros;
    int unsigned long segvs;
    int unsigned long segprots;
    process(vector<vma> a, int b)
    {
    	proc_id = b;
  	vmas = a;

     unmaps=0;
     maps=0;
     ins=0;
     outs=0;
     fins=0;
     fouts=0;
     zeros=0;
     segvs=0;
     segprots=0;
    }
};
vector<process*> proclist;

class Pager {
public:
virtual frame_t* select_victim_frame() = 0; // virtual base class
virtual frame_t * reset_age(frame_t * newframe)=0;
};

class FIFO: public Pager{
public:
	frame_t* select_victim_frame(){
		frame_t * res = &frame_table[HAND];
		HAND = (HAND+1)%MAX_FRAMES;
		// cout<<HAND<<endl;
		return res;
	}
	frame_t * reset_age(frame_t * newframe){
		return newframe;
	}

};
class RANDOM: public Pager{
public:
	frame_t* select_victim_frame(){
		frame_t * res = &frame_table[myrandom()];
		return res;
	}
	frame_t * reset_age(frame_t * newframe){
		return newframe;
	}

};

class CLOCK: public Pager{
public:
	frame_t* select_victim_frame(){

		frame_t * res = &frame_table[HAND];
		pte_t * itr = &proclist[res->proc_id]->page_table[res->vpage_id];
		// cout<<itr->referenced<<endl;
		while(itr->referenced==1){
			itr->referenced = 0;
			HAND = (HAND+1)%MAX_FRAMES;
			res = &frame_table[HAND];
			itr = &proclist[res->proc_id]->page_table[res->vpage_id];
			// cout<<HAND<< " HAND "<<endl;
		}
		// res = &frame_table[HAND];
		HAND = (HAND+1)%MAX_FRAMES;
		// cout<<HAND<<endl;
		return res;
	}
	frame_t * reset_age(frame_t * newframe){
		return newframe;
	}

};


class ESC: public Pager{
public:


	frame_t * reset_age(frame_t * newframe){
		return newframe;
	}

	frame_t* select_victim_frame(){

		bool cls[4] = { false,false,false,false};
		int hand[4] = {-1,-1,-1,-1};
		int ctr = 0;
		int itr_h = HAND ;
		for(int x = 0; x<MAX_FRAMES; x++){
			frame_t f = frame_table[itr_h];
			pte_t i = proclist[f.proc_id]->page_table[f.vpage_id];
			// printf("ref %d mod %d, class %d hand %d\n",i.referenced,i.modified,2*i.referenced + i.modified,HAND);
			if(hand[2*i.referenced + i.modified]==-1){
				hand[2*i.referenced + i.modified] = itr_h;
				if((2*i.referenced + i.modified) == 0){
					break;
				}
			}
			itr_h = (itr_h+1)%MAX_FRAMES;
			ctr++;

		}

		if(instr_ct-prev_reset>TAU){

			for(int x = 0; x<MAX_FRAMES; x++){
				frame_t f = frame_table[x];
				pte_t * i = &proclist[f.proc_id]->page_table[f.vpage_id];
				i->referenced = 0;
				// printf("reset %d\n",proclist[f.proc_id]->page_table[f.vpage_id].referenced);
			}
			prev_reset = instr_ct;
		}


		for(int i=0;i<4;i++)
		{
			// printf("class %d first %d\n",i,hand[i]);
			if(hand[i]!=-1){
				HAND = (hand[i]+1)%MAX_FRAMES;
				return &frame_table[hand[i]];
			}
		}


		
	}


};
class AGING: public Pager{
public:
	frame_t * reset_age(frame_t * newframe){
		// cout<<HAND<<" inside hand"<<endl;
		newframe->age = 0;
		return newframe;
	}
	frame_t* select_victim_frame(){

		int itr_h = HAND ;
		
		int res_hand=HAND;
		// printf("prev_reset\n");
		// fflush(stdout);
		for(int x = 0; x<MAX_FRAMES; x++){
			frame_t * f = &frame_table[itr_h];
			pte_t i = proclist[f->proc_id]->page_table[f->vpage_id];
			// printf("ref %d mod %d, class %d hand %d\n",i.referenced,i.modified,2*i.referenced + i.modified,HAND);
			f->age = f->age>>1;
			if(i.referenced==1)
			{
				f->age = (f->age | 0x80000000);
			}
			if(f->age<frame_table[res_hand].age){
				res_hand = itr_h;
			}
			itr_h = (itr_h+1)%MAX_FRAMES;

		}
		// printf("prev_reset\n");
		// fflush(stdout);
		p_HAND = res_hand;
		// cout<<HAND<<" outside hand"<<endl;
		// reset_age(frame_table);
		
		HAND = (res_hand +1)%MAX_FRAMES;
		for(int x = 0; x<MAX_FRAMES; x++){
			// printf("%d\n",x);
			// fflush(stdout);
			pte_t * i = &proclist[frame_table[x].proc_id]->page_table[frame_table[x].vpage_id];
			i->referenced = 0;


		}

		// printf("complete\n");
			// fflush(stdout);
		// printf("res hand %d\n",res_hand);
			// fflush(stdout);	
		return &frame_table[res_hand];

		
	}
	

};


class WSET: public Pager{
public:
	frame_t * reset_age(frame_t * newframe){
		// cout<<HAND<<" inside hand"<<endl;
		newframe->timestamp = instr_ct;
		return newframe;
	}
	frame_t* select_victim_frame(){

		int itr_h = HAND ;	
		int res_hand=HAND;

		for(int x = 0; x<MAX_FRAMES; x++){
			frame_t * f = &frame_table[itr_h];
			pte_t * i = &proclist[f->proc_id]->page_table[f->vpage_id];
			if(i->referenced==1)
			{
				
				f->timestamp = instr_ct;				
				i->referenced = 0;
			

			}
			else{

				if(instr_ct - f->timestamp>TAU){
					HAND = (itr_h +1)%MAX_FRAMES;
					return &frame_table[itr_h];
				}

				if(f->timestamp<frame_table[res_hand].timestamp){
				res_hand = itr_h;
			}

			
			

		}
		
		itr_h = (itr_h+1)%MAX_FRAMES;
		
	}
	HAND = (res_hand +1)%MAX_FRAMES;
		return &frame_table[res_hand];
}
	

};


Pager * THE_PAGER;
frame_t * allocate_frame_from_free_list(){

	if(free_pool.empty())
		{
			// printf("empty");
			return NULL;}
	else{
		frame_t * res = free_pool.front();
		// cout<<free_pool.size()<<endl;
		free_pool.pop_front();
		return res;
	}
}

frame_t *get_frame() {
frame_t *frame =  allocate_frame_from_free_list();
if (frame == NULL) frame = THE_PAGER->select_victim_frame();
 return frame;
}


vector<process*> input_preproc(){
	
	vector<process*> procs;
	vector<vma> vmas;
	
	

	while(getline(processes,process_line))
	{       
                if(process_line.length()==0)
					{break;}
				if(process_line[0]=='#')
					{continue;}

		
		stringstream streamline(process_line);
		streamline>>token;
		int process_no = stoi(token);
		int x = 0;
		while(x<process_no)
		{
		vmas.clear();
		getline(processes,process_line);
		
		if(process_line[0]=='#')
					{continue;}
		stringstream streamline(process_line);
		streamline>>token;
		int num_vmas = stoi(token);
		// cout<<num_vmas;
		int i = 0;
		while(i<num_vmas)
		{
		getline(processes,process_line);
		
		if(process_line[0]=='#')
					{continue;}
		stringstream streamline(process_line);
		streamline>>token;
		int a = stoi(token);
		streamline>>token;
		int b = stoi(token);
		streamline>>token;
		int c = stoi(token);
		streamline>>token;
		int d = stoi(token);
		vma temp_vma(a,b,c,d);
		vmas.push_back(temp_vma);
		i++;
	}
		process * p_temp = new process(vmas,x);
		procs.push_back(p_temp);
		x++;
	}
	break;
	
	}
	return procs;


}
bool get_next_instruction(string * operation,int * vpage){
	while(getline(processes,process_line))
	{       
                if(process_line.length()==0)
					{break;}
				if(process_line[0]=='#')
					{continue;}

		string temp;
		stringstream streamline(process_line);
		streamline>>*operation;
		streamline>>temp;
		*vpage = stoi(temp);
		return true;

	}
	
	return false;
	



}



void update_pte(pte_t * p, string mode, process * current_process)
{	p->referenced = 1;
	
	if(mode == "w"){
		if(p->write_protect){
			current_process->segprots++;
			g->cost += 420;
			if(ohno)
			printf(" SEGPROT\n");
			return;
		}
		p->modified = 1;
	}
	else if(mode == "r"){

	}
}

bool validate_frame(process * p,int vpage, int * wpt, int *fmap){
	for(auto i : p->vmas){
		if(vpage>=i.start && vpage<=i.end)
		{	*wpt = i.write_protected;
			*fmap = i.file_mapped;
			return false;
		}

	}
return true;
}

void print_ftable()
{
	printf("FT:");
	for(int i = 0; i < MAX_FRAMES; i++){
		if(frame_table[i].used){
			printf(" %d:%d",frame_table[i].proc_id,frame_table[i].vpage_id);
		}
			else{
				printf(" *");
			}
	}
	printf("\n");

}
void print_pstats(){


	for(auto proc: proclist){
	printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
 proc->proc_id,
 proc->unmaps, proc->maps, proc->ins, proc->outs,
 proc->fins, proc->fouts, proc->zeros,
 proc->segvs, proc->segprots);

	}

}
void print_pgtable(){


int pid = 0;
for(auto cp: proclist){
	printf("PT[%d]: ",pid);
	pid++;
	int ptid = 0;

	for(auto i: cp->page_table){

			pte_t *pte = &i;
			// cout<<pte->valid<< " id "<< pid-1 <<endl;
			if(!pte->valid)
			{	
				if(pte->paged_out)
				{
					printf("#");
				}
				else
				{
					printf("*");
				}
			}
			else{
				printf("%d:",ptid);
				if(pte->referenced)
				{
					printf("R");
				}
				else{
					printf("-");

				}
				if(pte->modified)
				{
					printf("M");
				}
				else{
					printf("-");

				}
				if(pte->paged_out)
				{
					printf("S");
				}
				else{
					printf("-");

				}
			}
			if(ptid<63){
			printf(" ");
			ptid++;
		}
			
		}
		printf("\n");
}

}


void simulation(){
string  operation;
int  vpage;
process * current_process = NULL;
while (get_next_instruction(&operation, &vpage)) {
	if(ohno)
	printf("%d: ==> %s %d\n",instr_ct,operation.c_str(),vpage);
	instr_ct++;
	g->g_ins++;
 // handle special case of “c” and “e” instruction
	if(operation == "c"){ 
		current_process = proclist[vpage];
		g->cs++;
		g->cost+= 130;
		continue;
	}
	if(operation == "e"){
		g->pes++;
		if(ohno)
		printf("EXIT current process %d\n", vpage);
		g->cost += 1250;
		for(int ex = 0; ex<MAX_VPAGES; ex++){
		// for(auto i: current_process->page_table){

			pte_t *pte = &current_process->page_table[ex];
			if(pte->valid){
				current_process->unmaps++;
				g->cost += 400;
				if(ohno)
				printf(" UNMAP %d:%d\n", frame_table[pte->f_no].proc_id,frame_table[pte->f_no].vpage_id);
				if(pte->file_mapped==1 && pte->modified==1){
					current_process->fouts++;
					g->cost += 2400;
					if(ohno)
					printf(" FOUT\n");
				}
				frame_table[pte->f_no].used = false;
				free_pool.push_back(&frame_table[pte->f_no]);
			}
			pte->paged_out = 0;
			pte->valid = 0;

			// cout<<i.valid<<" 999999999  ";
		}
		continue;
	}
 // now the real instructions for read and write
 pte_t *pte = &current_process->page_table[vpage];
 g->cost+=1;
 if ( ! pte->valid) {
 // this in reality generates the page fault exception and now you execute
 // verify this is actually a valid page in a vma if not raise error and next inst
 	int  write_protected_temp;
 	int  file_mapped_temp;
	bool segv = validate_frame(current_process,vpage, &write_protected_temp, &file_mapped_temp);
	if(segv){
		current_process->segvs++;
		g->cost += 340;
		if(ohno)
		printf(" SEGV\n");
		continue;
	}
 frame_t *newframe = get_frame();
 pte->valid = 1;
 pte->file_mapped = file_mapped_temp;
 pte->write_protect = write_protected_temp;
 // cout<<newframe->used<<"  test  "<<newframe->frame_id<<endl;
// printf("stop1\n");
			// fflush(stdout);
 if(newframe->used){
 	// printf("stop2\n");
			// fflush(stdout);
 	pte_t * victim = &proclist[newframe->proc_id]->page_table[newframe->vpage_id];
 	proclist[newframe->proc_id]->unmaps++;
 	g->cost += 400;
 	if(ohno)
 	printf(" UNMAP %d:%d\n",newframe->proc_id,newframe->vpage_id);
 	if(victim->modified){
 	if(victim->file_mapped){
 		g->cost += 2400;
 		proclist[newframe->proc_id]->fouts++;
 		if(ohno)
 		printf(" FOUT\n");
 	}
 	else{
 		g->cost += 2700;
 		proclist[newframe->proc_id]->outs++;
 		if(ohno)
 		printf(" OUT\n");
 	victim->paged_out=1;

 	}
 }
 	victim->valid = 0;
 	victim->modified = 0;

 }
 
 	
 	
 
 if(pte->file_mapped)
 {
 	g->cost += 2800;
 	current_process->fins++;
 	if(ohno)
 	printf(" FIN\n");
 }
 else if(pte->paged_out){
 	current_process->ins++;
 	g->cost += 3100;
 	if(ohno)
 	printf(" IN\n");
 }
 else if(!pte->paged_out && !pte->file_mapped){
 	current_process->zeros++;
 	g->cost += 140;
 	if(ohno)
 	printf(" ZERO\n");
 }

newframe->proc_id = current_process->proc_id;
 	newframe->vpage_id = vpage;
 	newframe->used = true;
 	// newframe->age = 0;
 	newframe = THE_PAGER->reset_age(newframe);
 	pte->f_no = newframe->frame_id;

 	// newframe->timestamp = instr_ct;
 	current_process->maps++;
 	g->cost += 300;
 	if(ohno)
 printf(" MAP %d\n",newframe->frame_id); 
 //-> figure out if/what to do with old frame if it was mapped
 // see general outline in MM-slides under Lab3 header and writeup below
 // see whether and how to bring in the content of the access page.
 }
	

 // frame_t * temp = &frame_table[pte->f_no];
 // temp->timestamp = instr_ct;
 // check write protection
 // simulate instruction execution by hardware by updating the R/M PTE bits
 update_pte(pte, operation, current_process);
 // bits based on operations.
// cout<<pte->referenced<<" simul  "<<endl;
 
}

}



int main (int argc, char **argv) {

	string choices;
    int opt;
    char algo;

    while ((opt = getopt(argc, argv, "f:a:o:")) != -1) {
        switch (opt) 
          {
            case 'f':
                int bits;
                sscanf(optarg, "%d", &MAX_FRAMES);
                break;
            case 'a': 
                sscanf(optarg, "%c", &algo);
                break;
            case 'o': 
                choices=optarg;
                break;
          }
    }


    for (int i = 0; i < choices.length(); i++) {
  
        // Print current character
        if(choices[i]=='O')
        {
        	ohno = true;
        }
        else if(choices[i]=='P')
        {
        	print_pt = true;
        }
        else if(choices[i]=='F')
        {
        	print_ft = true;
        }
        else if(choices[i]=='S')
        {
        	print_summary = true;
        }
    }

    switch (algo) 
          {

          	case 'f':
                THE_PAGER = new FIFO();
                break;
            case 'r':
                THE_PAGER = new RANDOM();
                break;
            case 'c':
                THE_PAGER = new CLOCK();
                break;
            case 'e':
                THE_PAGER = new ESC();
                break;
            case 'a':
                THE_PAGER = new AGING();
                break;
            case 'w':
                THE_PAGER = new WSET();
                break;

          }



	string filename = argv[optind];


	randf.open(argv[optind + 1]);
	string number_rands;
	getline(randf,number_rands);
	int n = stoi(number_rands);
	int temp;
	for(int i = 0; i<n; i++)
	{
		getline(randf,number_rands);
		temp = stoi(number_rands);
		randvals.push_back(temp);

	}


	processes.open(filename);
	// frame_t frame_table[MAX_FRAMES];
	frame_table =  new frame_t[MAX_FRAMES];

	// pte_t page_table[MAX_VPAGES]; 
	// THE_PAGER = new FIFO();
	// THE_PAGER = new RANDOM();
	// THE_PAGER = new CLOCK();
	// THE_PAGER = new ESC();
	// THE_PAGER = new AGING();
	// THE_PAGER = new WSET();
	proclist = input_preproc();
	
	for(int i=0;i<MAX_FRAMES;i++)
	{	frame_t temp_frame;
		temp_frame.used = false;
		temp_frame.frame_id = i;
		
		frame_table[i] = temp_frame;
		free_pool.push_back(&frame_table[i]);
		// cout<<temp_frame.used;
	}
	// for(auto i : free_pool)
	// 	{cout<<i->frame_id<<endl;}
	// frame_table[0].used = true;
	// for(int i=0;i<MAX_FRAMES;i++)
	// {	
	// 	cout<<frame_table[i].used<<endl;
	// }
	int pctr = 0;
	// for(auto i: proclist){
	// 	printf("process %d\n",pctr);
	// 	for (auto j: i.vmas){
	// 		printf("%d %d %d %d\n",j.start,j.end,j.write_protected,j.file_mapped);
	// 	}
	// 	pctr++;
	// }
	g = new globalstats();
	simulation();
	if(print_pt)
	print_pgtable();
	if(print_ft)
	print_ftable();
	if(print_summary){
	print_pstats();
	printf("TOTALCOST %lu %lu %lu %llu %lu\n", g->g_ins, g->cs, g->pes, g->cost, sizeof(pte_t));}
	}