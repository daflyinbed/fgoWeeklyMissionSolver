#include <iostream>
#include <vector>
#include <cstdio>
#include <string>

#include "rapidjson\filereadstream.h"
#include "rapidjson\document.h"
#include "rapidjson\pointer.h"
extern "C" {
	#include <glpk.h>
}
extern std::string Utf8ToGbk(const char *src_str);
int StageCount = 170;
int FeatureCount = 140;
int main() {
	
	//read Feature Data from json file
	FILE *fsf;
	fopen_s(&fsf,"stageFeatureData.json", "rb");

	char readBuffer[65536];
	rapidjson::FileReadStream is(fsf, readBuffer, sizeof(readBuffer));

	rapidjson::Document d;
	d.ParseStream(is);
	fclose(fsf);

	//read andObjectData from json file
	FILE * frod;
	fopen_s(&frod, "OrObjectData.json", "rb");

	rapidjson::FileReadStream srod(frod, readBuffer, sizeof(readBuffer));

	rapidjson::Document rod;
	rod.ParseStream(srod);
	fclose(frod);


	StageCount = d[1].GetArray().Size();
	int feature_monster_size = d[0].GetArray()[0].GetArray().Size();
	int feature_boss_size = d[0].GetArray()[1].GetArray().Size();
	int feature_area_size = d[0].GetArray()[2].GetArray().Size();
	FeatureCount = feature_monster_size + feature_boss_size + feature_area_size;

	//GLPK
	glp_prob *lp;
	lp = glp_create_prob();
	glp_set_obj_dir(lp, GLP_MIN);
	
	//GLPK 将各辅助变量看作行(row)，将原有的变量看作列(column)
	glp_add_cols(lp, StageCount);//每关一个原有变量
	glp_add_rows(lp, rod.Size());//每个任务一个辅助变量
	
	//原有变量
	for (int i = 0; i < StageCount; i++) {
		glp_set_col_name(lp, i + 1, d[1].GetArray()[i].GetArray()[1].GetString());
		glp_set_col_bnds(lp, i + 1, GLP_LO, 0, 0);
		glp_set_col_kind(lp, i + 1, GLP_IV);
	}
	   	  
	//辅助变量
	for (int i = 0; i < rod.Size(); i++) {
		std::string temp = "";
		for (auto itr = rod[i].GetArray().Begin()+1; itr != rod[i].GetArray().End(); ++itr) {
			temp = temp + itr->GetString() + ",";
		}
		//std::cout << temp << std::endl;
		glp_set_row_name(lp, i + 1, temp.c_str());
		glp_set_row_bnds(lp, i + 1, GLP_LO, rod[i].GetArray()[0].GetInt(), 0);
		
	}

	//求最小cost
	for (int i = 0; i < StageCount; i++) {
		glp_set_obj_coef(lp, i + 1, d[1].GetArray()[i].GetArray()[3].GetInt());
	}

	//constrant_matrix
	int c = (rod.Size()) * StageCount;
	int *ia = new int[c+1];//ia[k] is row index(辅助变量)
	int *ja = new int[c+1];//ja[k] is column index(原有变量)
	double *ar = new double[c+1];
	for (int i = 0; i < c; i++) {
		ia[i + 1] = (i + StageCount) / StageCount;
		ja[i + 1] = i % StageCount + 1;
		int t = 0;
		int c = ia[i + 1] - 1;
		for (auto it = rod[c].GetArray().Begin() + 1; it != rod[c].GetArray().End(); ++it) {//遍历多目标数组
			//std::cout << Utf8ToGbk(it->GetString()) << std::endl;
			std::string t0 = Utf8ToGbk(it->GetString());
			std::string t1;
			for (int iter = 0; iter < d[0].GetArray()[0].GetArray().Size(); ++iter) {//遍历小怪特性数组
				t1 = Utf8ToGbk(d[0].GetArray()[0].GetArray()[iter].GetString());
				if (0 == t0.compare(t1)) {
					t+= d[1].GetArray()[ja[i + 1] - 1].GetArray()[4].GetString()[iter] - '0';
					//std::cout << t0 << "	" << t;
					goto l;
				}
			}
			for (int iter = 0; iter < d[0].GetArray()[1].GetArray().Size(); ++iter) {//遍历从者特性数组
				t1 = Utf8ToGbk(d[0].GetArray()[1].GetArray()[iter].GetString());
				if (0 == t0.compare(t1)) {
					//std::cout << d[1].GetArray()[ja[i + 1] - 1].GetArray()[4].GetString()[iter+feature_monster_size] << std::endl;
					t += d[1].GetArray()[ja[i + 1] - 1].GetArray()[4].GetString()[iter+feature_monster_size] - '0';
					//std::cout << t0 << "	" << t;
					goto l;
				}
			}
			for (int iter = 0; iter < d[0].GetArray()[2].GetArray().Size(); ++iter) {//遍历场地特性数组
				t1 = Utf8ToGbk(d[0].GetArray()[2].GetArray()[iter].GetString());
				if (0 == t0.compare(t1)) {
					t += d[1].GetArray()[ja[i + 1] - 1].GetArray()[4].GetString()[iter+feature_monster_size+feature_boss_size] - '0';
					//std::cout << t0 << "	" << t;
					goto l;
				}
			}
		l:
			std::cout << t << std::endl;
		}
		ar[i + 1]=t;			
		std::cout << i + 1 << "ia:" << ia[i + 1] << "	ja:" << ja[i + 1] << "	ar:" << ar[i + 1] << std::endl;
	}

	glp_load_matrix(lp, c, ia, ja, ar);


	//运算
	glp_simplex(lp, NULL);
	glp_intopt(lp, NULL);

	//输出
	int o;
	o = glp_print_mip(lp, "sol.txt");
	std::cout << o;
	std::cin.get();
}
