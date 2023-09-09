# Cloud Deployment Tool
ChannelUE provides a set of tools for cloud deployment. After configuration, you can deploy to the cloud with one click, or use the packaging, uploading, or cloud launching tools separately.

The tools are implemented based on Docker and Kubernetes. It is recommended to learn the basic concepts of [Docker](https://docs.docker.com/get-started/overview) and [Kubernetes](https://kubernetes.io/docs/concepts/overview/what-is-kubernetes) first.

## Environment Requirements
- Windows 10/11
- Unreal Engine 4.27.2 (for building Linux server target)
- [Linux cross-compilation toolchain](https://docs.unrealengine.com/4.27/en-US/SharingAndReleasing/Linux/GettingStarted/)
- [Docker Desktop for Windows](https://docs.docker.com/desktop/windows/install)
- Kubernetes command line tool [kubectl](https://kubernetes.io/docs/reference/kubectl)
- Cloud container service based on Kubernetes, such as:
  - Amazon Elastic Kubernetes Service [EKS](https://aws.amazon.com/eks)
  - Google Kubernetes Engine [GKE](https://cloud.google.com/kubernetes-engine)
  - Microsoft Azure Kubernetes Service [AKS](https://azure.microsoft.com/en-us/services/kubernetes-service)
  - Alibaba Cloud Container Service [ACK](https://www.aliyun.com/product/containerservice)
  - Tencent Cloud Container Service [TKE](https://cloud.tencent.com/product/tke)
- Container image repository, such as:
  - [Docker Hub](https://hub.docker.com)
  - [Amazon Elastic Container Registry](https://aws.amazon.com/ecr)
  - [Google Container Registry](https://cloud.google.com/container-registry)
  - [Microsoft Azure Container Registry](https://azure.microsoft.com/en-us/services/container-registry)
  - Alibaba Cloud Container Registry [ACR](https://www.aliyun.com/product/acr)
  - Tencent Cloud Container Registry [TCR](https://cloud.tencent.com/product/tcr)

## Step 1: Packaging
The packaging process is divided into two steps: first, use the [Package Project](https://docs.unrealengine.com/4.27/en-US/Basics/Projects/Packaging/) function of Unreal Engine to build the Linux Server target; then use `docker build` to build channeld and the game server images.

When packaging, the packaging settings in the Project Settings will be used. Click `Open Packaging Settings` to jump to the settings panel.

>Note: Turn off the `Enable Compatible Recompilation` in the dropdown menu of the plugin before packaging, and **re-generate the replication code**. This ensures that the generated classes with the same names are used for each packaging, avoiding compatibility issues.

`Channeld Remote Image Tag` and `Server Remote Image Tag` are the image tags used for channeld and the game server, and will be used in packaging, uploading and deployment. The format of the image tag is: *base address of the container image repository*/*repository name*/*image name*:*version number*. The following are valid examples of image tags:
```
// Docker Hub (base address can be omitted)
channeld/tps-channeld:latest

// AWS ECR
123456789012.dkr.ecr.us-east-1.amazonaws.com/channeld/channeld:1.2.3

// Google Container Registry
gcr.io/channeld/channeld:1.2.3

// Microsoft Azure Container Registry
channeld.azurecr.io/channeld/channeld:1.2.3

// Alibaba Cloud
registry.cn-shanghai.aliyuncs.com/channeld/channeld:1.2.3

// Tencent Cloud
ccr.ccs.tencentyun.com/channeld/tps-server:0.6.0
```

>Tip: If no version number is specified, `latest` will be used by default.

## Step 2: Uploading
The uploading step will upload the images built in step 1 to the container image repository.

If the container image repository is private, you need to fill in `Registry Username` and `Registry Password` to log in. If you don't want to save this information in plain text, you can also use the `docker login` command to log in to the repository manually, and then click `Upload` to upload.

>Tip: `Registry Username` and `Registry Password` are also used to generate `Image Pull Secret` in step 3.

## Step 3: Deployment
The meaning of each setting in the deployment step is as follows:
- `Target Cluster Context`: The context of the Kubernetes cluster to use. Click `Detect` to get and fill in the current context. You can use the `kubectl config get-contexts` command to view all available contexts.
- `Namespace`: The Kubernetes namespace to deploy to. Click `Detect` to get and fill in the current namespace. You can use the `kubectl get namespaces` command to view all available namespaces.
- `YAML Template`: The YAML template file used for deployment, which contains the Deployment and Service of `channeld`, `grafana`, and `prometheus`.
- `Image Pull Secret`: Secret used to pull images from a private repository. If a public repository is used, it can be left blank. After filling in `Image Pull Secret`, click `Generate` on the right, and a Secret will be created in the cluster according to the `Channeld Remote Image Tag`, `Target Cluster Context`, `Namespace`, `Registry Username` and `Registry Password` that have been filled in.
- `channeld Launch Args`: Launch arguments for channeld. By default, it uese the same launch arguments in the Editor Settings.
- `Main Server Group`: Configuration of the main server (group).
  - `Enabled`: Whether to enable. When turned off, the main server will not be deployed.
  - `Server Num`: Number of servers. Generally, only one main server is needed.
  - `Server Map`: Map name. The current map is used by default.
  - `Server View Class`: Channel data view class. By default, it uses the view class in the Project Settings.
  - `YAML Template`: YAML template file used for UE server deployment.
  - `Additional Args`: Additional launch arguments.
- `Spatial Server Group`: Configuration of the spatial server (group).
  - `Enabled`: Whether to enable. When turned off, the spatial server will not be deployed. If you are running a single game server, you should turn off the spatial server.
  - Other fields are the same as above.

>NOTE: When using Tencent Cloud Container Image Service, please use `Template/DeploymentTencentCloud.yaml` under the ChanneldUE plugin directory as `YAML Template`.

After clicking the `Deploy` button, the final YAML file used for `kubectl apply` will be generated according to the YAML template file and above settings.

After the deployment is successful, the URL of Grafana will be displayed, which can be used to view the running metrics of the server. The default username and password are both `admin`.

>Tip: Grafana will download the dashboard file from github. If timeout, the Grafana service can still start normally, but the dashboard is empty. If this is your case, you should manually import the dashboard file in Grafana ([download address](https://raw.githubusercontent.com/metaworking/channeld/master/grafana/dashboard.json)).

### About Image Pull Secret
The `Generate` function on the right of `Image Pull Secret` only supports creating Secrets based on the username and password of the image repository. You can still use [kubectl](https://kubernetes.io/docs/concepts/configuration/secret/#creating-a-secret), or the console of the cloud service provider to create Secrets, and then fill them in the `Image Pull Secret` text box.

## One-Click Deployment
After all the above steps are completed successfully, click the `One-Click Deploy` button to automatically execute the three steps to accelerate the cloud deployment workflow.

Click the `Shut Down` button to shut down the currently running deployment, and destroy the corresponding cloud resources.

## Troubleshooting
### Packaging
- The console outputs `ERROR: error during connect: this error may indicate that the docker daemon is not running`, please make sure that `Docker Desktop` is running.

### Uploading
- The console outputs `Please login to the RepoUrl first.`, indicating that you need to log in when uploading the image to the repository. Please enter the username and password of the image repository as prompted in the console.

### Deployment
- The `Output Log` window outputs `Something wrong with xxx pod`, please further troubleshoot according to the pod status file below.
- If `ImagePullBackOff` or `ErrImagePull` appears in the pod status, please check whether the Secret name filled in the `Image Pull Secret` field is correct.
- Grafana cannot be accessed: Please wait patiently for the dashboard file to be downloaded or timeout. 
