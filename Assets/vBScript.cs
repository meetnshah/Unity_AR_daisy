using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using Vuforia;

public class vBScript : MonoBehaviour
{
    private GameObject vButtonObject;
    // Start is called before the first frame update
    void Start()
    {
        vButtonObject = GameObject.Find("actionButton");
        vButtonObject.GetComponent<VirtualButtonBehaviour>().RegisterOnButtonPressed(OnButtonPressed);


    }

    public void OnButtonPressed(VirtualButtonBehaviour vb)
    {
        Debug.Log("Button Down");
    }



    public void OnButtonReleased(VirtualButtonBehaviour vb)
    {
        Debug.Log("Button Released");
    }
}


    