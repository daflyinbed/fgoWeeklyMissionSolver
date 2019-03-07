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

	/*
	std::cout << Utf8ToGbk(d[1].GetArray()[0].GetArray()[0].GetString());
	std::cin.get();
	*/

	//read ObjectData from json file 
	FILE * fod;
	fopen_s(&fod, "ObjectData.json", "rb");

	rapidjson::FileReadStream sod(fod, readBuffer, sizeof(readBuffer));

	rapidjson::Document od;
	od.ParseStream(sod);
	fclose(fod);
	
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
	glp_add_rows(lp, FeatureCount);//每个特性一个辅助变量
	glp_add_rows(lp, rod.Size());//每个多目标任务一个辅助变量
	
	//原有变量
	for (int i = 0; i < StageCount; i++) {
		glp_set_col_name(lp, i + 1, d[1].GetArray()[i].GetArray()[1].GetString());
		glp_set_col_bnds(lp, i + 1, GLP_LO, 0, 0);
		glp_set_col_kind(lp, i + 1, GLP_IV);
	}
	   	  
	//辅助变量
	for (int i = 0; i < FeatureCount+ rod.Size(); i++) {
		if (i < feature_monster_size) {//monster
			std::string temp = d[0].GetArray()[0].GetArray()[i].GetString();
			glp_set_row_name(lp, i + 1, temp.c_str());
			temp = "/" + temp;
			glp_set_row_bnds(lp, i + 1, GLP_LO, rapidjson::Pointer(temp.c_str()).Get(od)->GetInt(), 0);
		}
		else if (i < feature_monster_size+feature_boss_size) {//boss
			std::string temp = d[0].GetArray()[1].GetArray()[i-feature_monster_size].GetString();
			glp_set_row_name(lp, i + 1, temp.c_str());
			temp = "/" + temp;
			glp_set_row_bnds(lp, i + 1, GLP_LO, rapidjson::Pointer(temp.c_str()).Get(od)->GetInt(), 0);
		}
		else if(i<FeatureCount){//area
			std::string temp = d[0].GetArray()[2].GetArray()[i-feature_monster_size-feature_boss_size].GetString();
			glp_set_row_name(lp, i + 1, temp.c_str());
			temp = "/" + temp;
			glp_set_row_bnds(lp, i + 1, GLP_LO, rapidjson::Pointer(temp.c_str()).Get(od)->GetInt(), 0);
		}
		else {//多目标任务
			std::string temp = "";
			for (auto itr = rod[i - FeatureCount].GetArray().Begin()+1; itr != rod[i - FeatureCount].GetArray().End(); ++itr) {
				temp = temp + itr->GetString() + ",";
			}
			//std::cout << temp << std::endl;
			glp_set_row_name(lp, i + 1, temp.c_str());
			glp_set_row_bnds(lp, i + 1, GLP_LO, rod[i - FeatureCount].GetArray()[0].GetInt(), 0);
		}
	}

	//求最小cost
	for (int i = 0; i < StageCount; i++) {
		glp_set_obj_coef(lp, i + 1, d[1].GetArray()[i].GetArray()[3].GetInt());
	}

	//constrant_matrix
	int c = (FeatureCount + rod.Size()) * StageCount;
	int *ia = new int[c+1];//ia[k] is row index(辅助变量)
	int *ja = new int[c+1];//ja[k] is column index(原有变量)
	double *ar = new double[c+1];
	for (int i = 0; i < c; i++) {
		ia[i + 1] = (i + StageCount) / StageCount;
		ja[i + 1] = i % StageCount + 1;
		if (i < FeatureCount * StageCount) {
			ar[i + 1] = d[1].GetArray()[ja[i + 1] - 1].GetArray()[4].GetString()[ia[i + 1] - 1] - '0';
		}
		else {
			int t = 0;
			int c = ia[i + 1] - FeatureCount - 1;
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
				//std::cout << t << std::endl;
			}
			ar[i + 1]=t;			
		}
		//std::cout << i + 1 << "ia:" << ia[i + 1] << "	ja:" << ja[i + 1] << "	ar:" << ar[i + 1] << std::endl;
	}
	/*
	for (int j = 0; j < rod.Size();j++) {
		std::cout << rod[j].GetArray().Size() << std::endl;
		for (int k = 1; k < rod[j].GetArray().Size(); k++) {
			int s = 1;
			while (s <= StageCount) {
				++s;
				++i;
				ia[i] = FeatureCount + j+1;
				ja[i] = (i - 1) % StageCount + 1;
				int index;
				for (int iter = 0; iter < d[0].GetArray()[0].GetArray().Size(); ++iter) {
					if (!strcmp(rod[j].GetArray()[k].GetString(),d[0].GetArray()[0].GetArray()[iter].GetString())) {
						index = iter;
						goto l1;
					}
				}
				for (int iter = 0; iter < d[0].GetArray()[1].GetArray().Size(); ++iter) {
					if (!strcmp(rod[j].GetArray()[k].GetString(), d[0].GetArray()[1].GetArray()[iter].GetString())) {
						index = iter;
						goto l1;
					}
				}
				for (int iter = 0; iter < d[0].GetArray()[1].GetArray().Size(); ++iter) {
					if (!strcmp(rod[j].GetArray()[k].GetString(), d[0].GetArray()[1].GetArray()[iter].GetString())) {
						index = iter;
						goto l1;
					}
				}
			l1:
				ar[i] = ar[i + 1] = d[1].GetArray()[ja[i] - 1].GetArray()[4].GetString()[index] - '0';
				std::cout << i << "ia:" << ia[i] << "	ja:" << ja[i] << "	ar:" << ar[i] << std::endl;
			}
			std::cout << std::endl;
		}
	}
	*/

	glp_load_matrix(lp, c, ia, ja, ar);


	//运算
	glp_simplex(lp, NULL);
	glp_intopt(lp, NULL);

	//输出
	/*
	int *a = new int[FeatureCount + StageCount + 1];
	for (int i = 1; i < FeatureCount + 1; i++) {
		a[i] = i;
	}
	for (int i = 1 + FeatureCount; i < FeatureCount + StageCount + 1; i++) {
		a[i] = i;
	}
	*/
	int o;
	//o = glp_print_ranges(lp, 0, NULL, 0, "sol.txt");
	o = glp_print_mip(lp, "sol.txt");
	std::cout << o;
	std::cin.get();
}
