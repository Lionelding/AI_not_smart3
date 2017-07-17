//20170609 Backup match algorithm

        	if (object_prenum>0){//frame>2
        		int preIndex=0;
        		for (preIndex=0; preIndex<object_prenum; preIndex++){
            		printf("idx_prestore[p]: %i\n", idx_prestore[preIndex]);
            		printf("degree: %0.0f\n", box_full[idx_prestore[preIndex]].flow.degree);
            		int preflow=box_full[idx_prestore[p]].flow.degree;
//            		if (abs(preflow-average_result)<30){
//
//            		}
            		int possibleDirection=preflow/45;
            		//int match=0;

            		switch(possibleDirection){

            		case 0: //0-45

            		//match=objectMatch(1, box_full[idx_prestore[preIndex]].classtype, box_para[idx_store[p]][4], idx_prestore[preIndex], idx_store[p]);
            		//match=objectMatch(-12, box_full[idx_prestore[preIndex]].classtype, box_para[idx_store[p]][4], idx_prestore[preIndex], idx_store[p]);

            		//box_full[idx_prestore[preIndex]].index
            		break;

            		case 1: //90-45

//					-12
//					-13
            		break;
            		case 2: //90-135

//					-13
//
//					-14
            		break;
            		case 3: //135-180

//					-14
//					-1
            		break;
            		case 4: //180-225

//					-1
//					+12
            		break;
            		case 5: //225-270
//					+12
//					+13
            		break;
            		case 6: //270-315
//					+13
//					+14
            		break;
            		case 7://315-360
//					+14
//					+1
            		break;
            		default: //stay

            		break;


            		//box_full[idx_prestore[p]+1, +13, +14]

            		}



        		}
        	}