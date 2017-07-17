    		//TODO: get rid of matched index
        	if (object_prenum>0){//frame>2

        		int preIndex=0;
        		for (preIndex=0; preIndex<object_prenum; preIndex++){

            		printf("Pre: idx_prestore[p]: %i degree: %0.0f mag: %0.0f objectIndex: %i\n", idx_prestore[preIndex], box_full[idx_prestore[preIndex]].flow.degree, box_full[idx_prestore[preIndex]].flow.magnitude, box_full[idx_prestore[preIndex]].objectIndex);
            		printf("Pre: idx_prestore[p]: %i degree: %0.0f mag: %0.0f objectIndex: %i\n", head->data, box_full[idx_prestore[preIndex]].flow.degree, box_full[idx_prestore[preIndex]].flow.magnitude, box_full[idx_prestore[preIndex]].objectIndex);

            		int preFlow=box_full[idx_prestore[preIndex]].flow.degree;
            		int preMag=box_full[idx_prestore[preIndex]].flow.magnitude;


            		if ((abs(preFlow-average_result.degree)<30) || (abs(preMag-average_result.magnitude)<3)){

                		match=objectMatch(box_full[idx_prestore[preIndex]].classtype, box_para[idx_store[p]][4], idx_prestore[preIndex], idx_store[p]);
                		if(match==1){
                    		box_para[idx_store[p]][9]=box_full[idx_prestore[preIndex]].objectIndex;

                    		//TODO: change the assumption what has been detected will be detected as well in the future
                    		Boxflow nullflow=putNullInsideBox();
                    		printf("%i matches with %i, with objectIndex: %i\n", idx_store[p], idx_prestore[preIndex], box_full[idx_prestore[preIndex]].objectIndex);
                    		box_full[idx_prestore[preIndex]]=nullflow;

                    		break;
                		}

            		}

        		}
        	}