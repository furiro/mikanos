int GetVersion(){
    return 1;
}

int GetCpuNumber(int pl_number){
    return (pl_number + 2) % 3;
}
