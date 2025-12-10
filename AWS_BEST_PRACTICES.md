# AWS Best Practices for OSv Deployment

## Overview

This guide provides AWS-specific best practices for deploying and operating OSv unikernels in AWS environments. It complements the Amazon Q Code Review recommendations and helps teams leverage AWS services effectively with OSv.

## Table of Contents

1. [Deployment Options](#deployment-options)
2. [EC2 Instance Configuration](#ec2-instance-configuration)
3. [Networking and Security](#networking-and-security)
4. [Performance Optimization](#performance-optimization)
5. [Monitoring and Logging](#monitoring-and-logging)
6. [CI/CD Integration](#cicd-integration)
7. [Cost Optimization](#cost-optimization)
8. [High Availability](#high-availability)

## Deployment Options

### 1. Amazon EC2

OSv can run on Amazon EC2 as a guest virtual machine:

**Supported Instance Types**:
- **General Purpose**: T3, T4g, M5, M6g, M6i
- **Compute Optimized**: C5, C6g, C6i (best for CPU-intensive workloads)
- **Memory Optimized**: R5, R6g, R6i (best for memory-intensive workloads)
- **Burstable**: T3a, T4g (cost-effective for variable workloads)

**Recommended Instance Types for OSv**:
```
Small workloads:   t3.micro, t3.small, t4g.micro
Medium workloads:  c5.large, c6i.large, m5.large
Large workloads:   c5.2xlarge, c6i.2xlarge, m5.2xlarge
```

### 2. AWS Lambda (Firecracker)

OSv can run on AWS Lambda using Firecracker microVMs:

**Benefits**:
- Pay-per-use pricing
- Automatic scaling
- No server management
- Fast cold starts (OSv boots in ~5ms)

**Considerations**:
- Custom runtime required
- Limited execution time (15 minutes max)
- Stateless execution model

### 3. AWS Fargate with Firecracker

Run OSv containers on AWS Fargate:

**Benefits**:
- Serverless container execution
- Native container orchestration
- No EC2 management

### 4. Amazon Lightsail

For simple deployments:

**Benefits**:
- Simplified management
- Predictable pricing
- Integrated networking

## EC2 Instance Configuration

### AMI Creation

Create a custom AMI with OSv:

```bash
# Build OSv image
./scripts/build image=<your-app>

# Convert to AWS-compatible format
qemu-img convert -f raw -O vpc build/release/usr.img osv.vpc

# Upload to S3
aws s3 cp osv.vpc s3://your-bucket/osv.vpc

# Import as AMI
aws ec2 import-image \
    --description "OSv Unikernel" \
    --disk-containers "Format=vpc,UserBucket={S3Bucket=your-bucket,S3Key=osv.vpc}"
```

### Instance Metadata Service (IMDS)

Configure OSv to use AWS IMDS:

```cpp
// Fetch instance metadata (simplified example)
#include <curl/curl.h>
#include <string>

std::string get_instance_id() {
    CURL* curl = curl_easy_init();
    if (!curl) return "";
    
    std::string url = "http://169.254.169.254/latest/meta-data/instance-id";
    std::string response;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, 
        [](void* contents, size_t size, size_t nmemb, void* userp) {
            ((std::string*)userp)->append((char*)contents, size * nmemb);
            return size * nmemb;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    return (res == CURLE_OK) ? response : "";
}
```

### User Data

Bootstrap OSv instances with user data:

```yaml
#!/usr/bin/osv
--env=APP_CONFIG=/etc/config.json
--env=AWS_REGION=us-east-1
/myapp --port=8080
```

## Networking and Security

### VPC Configuration

**Best Practices**:
1. Deploy OSv in private subnets
2. Use NAT Gateway for outbound internet access
3. Implement network segmentation
4. Use VPC Flow Logs for monitoring

**Example VPC Layout**:
```
VPC (10.0.0.0/16)
├── Public Subnet (10.0.1.0/24)
│   └── NAT Gateway
├── Private Subnet (10.0.10.0/24)
│   └── OSv Application Instances
└── Private Subnet (10.0.11.0/24)
    └── Database (RDS)
```

### Security Groups

Configure security groups for OSv instances:

```yaml
# Application Security Group
InboundRules:
  - Protocol: TCP
    Port: 8080
    Source: LoadBalancerSecurityGroup
  - Protocol: TCP
    Port: 22
    Source: BastionSecurityGroup  # For debugging only

OutboundRules:
  - Protocol: TCP
    Port: 443
    Destination: 0.0.0.0/0  # HTTPS to AWS services
  - Protocol: TCP
    Port: 3306
    Destination: DatabaseSecurityGroup
```

### IAM Roles and Policies

Grant OSv instances appropriate permissions:

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": [
        "s3:GetObject",
        "s3:PutObject"
      ],
      "Resource": "arn:aws:s3:::your-bucket/*"
    },
    {
      "Effect": "Allow",
      "Action": [
        "secretsmanager:GetSecretValue"
      ],
      "Resource": "arn:aws:secretsmanager:*:*:secret:app/*"
    },
    {
      "Effect": "Allow",
      "Action": [
        "cloudwatch:PutMetricData"
      ],
      "Resource": "*"
    }
  ]
}
```

### AWS Systems Manager (SSM)

Enable SSM for secure shell access:

```bash
# Install SSM agent in OSv (future enhancement)
# Currently, use SSH through bastion host

# Access via Session Manager (no SSH keys needed)
aws ssm start-session --target i-1234567890abcdef0
```

### Secrets Management

Use AWS Secrets Manager for sensitive data:

```python
#!/usr/bin/env python3
import boto3
import json

def get_secret(secret_name):
    client = boto3.client('secretsmanager', region_name='us-east-1')
    response = client.get_secret_value(SecretId=secret_name)
    return json.loads(response['SecretString'])

# Retrieve at startup
db_credentials = get_secret('prod/database/credentials')
```

## Performance Optimization

### Instance Type Selection

Choose instance types based on workload:

**CPU-Intensive Workloads**:
- Use Compute Optimized instances (C5, C6i)
- Enable Enhanced Networking (SR-IOV)
- Use Placement Groups for low-latency

**Memory-Intensive Workloads**:
- Use Memory Optimized instances (R5, R6i)
- Consider R6i instances with DDR5 memory

**Network-Intensive Workloads**:
- Use instances with Enhanced Networking
- Enable ENA (Elastic Network Adapter)
- Consider 100 Gbps networking instances

### EBS Optimization

Configure EBS volumes for performance:

```yaml
# High-performance root volume
VolumeType: gp3  # General Purpose SSD
Size: 100 GB
Iops: 16000  # Provisioned IOPS
Throughput: 1000  # MiB/s
```

**Best Practices**:
- Use gp3 volumes for better price/performance
- Provision IOPS for consistent performance
- Use io2 Block Express for highest performance
- Enable EBS optimization on instance

### Enhanced Networking

Enable Enhanced Networking for better network performance:

```bash
# Verify ENA support
aws ec2 describe-instances --instance-id i-1234567890abcdef0 \
    --query 'Reservations[].Instances[].EnaSupport'

# Enable ENA (if not already enabled)
aws ec2 modify-instance-attribute --instance-id i-1234567890abcdef0 \
    --ena-support
```

**Benefits**:
- Higher bandwidth (up to 100 Gbps)
- Lower latency
- Lower jitter
- Higher packets per second (PPS)

### Placement Groups

Use placement groups for latency-sensitive applications:

```bash
# Create cluster placement group (low-latency)
aws ec2 create-placement-group \
    --group-name osv-cluster \
    --strategy cluster

# Launch instances in placement group
aws ec2 run-instances \
    --placement GroupName=osv-cluster \
    ...
```

**Strategies**:
- **Cluster**: Low-latency, high-throughput (same AZ)
- **Partition**: Reduce correlated failures (different racks)
- **Spread**: Maximum availability (different hardware)

## Monitoring and Logging

### Amazon CloudWatch

Monitor OSv instances with CloudWatch:

**Custom Metrics**:
```python
import boto3

cloudwatch = boto3.client('cloudwatch')

def publish_metric(metric_name, value, unit='Count'):
    cloudwatch.put_metric_data(
        Namespace='OSv/Application',
        MetricData=[{
            'MetricName': metric_name,
            'Value': value,
            'Unit': unit
        }]
    )

# Example usage
publish_metric('RequestsProcessed', 100, 'Count')
publish_metric('ResponseTime', 50.5, 'Milliseconds')
```

**Key Metrics to Monitor**:
- CPU Utilization
- Memory Usage
- Network In/Out
- Disk I/O
- Application-specific metrics

### CloudWatch Logs

Stream logs to CloudWatch:

```python
import boto3
import time

logs = boto3.client('logs')
log_group = '/osv/application'
log_stream = f'instance-{instance_id}'

def send_log(message):
    logs.put_log_events(
        logGroupName=log_group,
        logStreamName=log_stream,
        logEvents=[{
            'timestamp': int(time.time() * 1000),
            'message': message
        }]
    )
```

### AWS X-Ray

Instrument OSv applications with X-Ray for distributed tracing:

```cpp
// Example: X-Ray integration (requires SDK)
#include <aws/xray/xray.h>

void process_request() {
    auto segment = XRay::beginSegment("ProcessRequest");
    
    // Do work
    auto subsegment = segment->beginSubsegment("DatabaseQuery");
    query_database();
    subsegment->close();
    
    segment->close();
}
```

### CloudWatch Alarms

Set up alarms for critical metrics:

```yaml
HighCPUAlarm:
  Type: AWS::CloudWatch::Alarm
  Properties:
    AlarmName: OSv-HighCPU
    MetricName: CPUUtilization
    Namespace: AWS/EC2
    Statistic: Average
    Period: 300
    EvaluationPeriods: 2
    Threshold: 80
    ComparisonOperator: GreaterThanThreshold
    AlarmActions:
      - !Ref SNSTopic
```

## CI/CD Integration

### AWS CodePipeline

Automate OSv image builds and deployments:

```yaml
# buildspec.yml
version: 0.2
phases:
  install:
    commands:
      - apt-get update
      - apt-get install -y build-essential
  build:
    commands:
      - ./scripts/build image=myapp
      - qemu-img convert -f raw -O vpc build/release/usr.img osv.vpc
  post_build:
    commands:
      - aws s3 cp osv.vpc s3://$ARTIFACT_BUCKET/osv-$CODEBUILD_BUILD_NUMBER.vpc
artifacts:
  files:
    - osv.vpc
```

### AWS CodeBuild

Build OSv images in CodeBuild:

```yaml
CodeBuildProject:
  Type: AWS::CodeBuild::Project
  Properties:
    Name: osv-builder
    Source:
      Type: GITHUB
      Location: https://github.com/your-org/osv
    Environment:
      Type: LINUX_CONTAINER
      ComputeType: BUILD_GENERAL1_LARGE
      Image: aws/codebuild/standard:5.0
    ServiceRole: !GetAtt CodeBuildRole.Arn
```

### AWS CodeDeploy

Deploy OSv applications:

```yaml
# appspec.yml
version: 0.0
os: linux
hooks:
  ApplicationStop:
    - location: scripts/stop_application.sh
      timeout: 300
  ApplicationStart:
    - location: scripts/start_application.sh
      timeout: 300
  ValidateService:
    - location: scripts/validate_service.sh
      timeout: 300
```

### GitHub Actions with AWS

Integrate GitHub Actions with AWS:

```yaml
name: Build and Deploy OSv
on: [push]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Configure AWS credentials
        uses: aws-actions/configure-aws-credentials@v1
        with:
          aws-access-key-id: ${{ secrets.AWS_ACCESS_KEY_ID }}
          aws-secret-access-key: ${{ secrets.AWS_SECRET_ACCESS_KEY }}
          aws-region: us-east-1
      
      - name: Build OSv
        run: ./scripts/build image=myapp
      
      - name: Upload to S3
        run: |
          qemu-img convert -f raw -O vpc build/release/usr.img osv.vpc
          aws s3 cp osv.vpc s3://artifacts/osv-${GITHUB_SHA}.vpc
```

## Cost Optimization

### Right-Sizing

Choose appropriate instance sizes:

**Strategies**:
1. Start with smaller instances
2. Monitor utilization metrics
3. Scale up if consistently high utilization
4. Use Compute Optimizer for recommendations

### Savings Plans and Reserved Instances

Reduce costs for predictable workloads:

```bash
# View Savings Plans recommendations
aws ce get-savings-plans-purchase-recommendation \
    --savings-plans-type COMPUTE_SP \
    --term-in-years ONE_YEAR \
    --payment-option NO_UPFRONT

# Purchase Savings Plan
aws savingsplans purchase-savings-plan \
    --savings-plan-offering-id <offering-id> \
    --commitment 10.0
```

**Comparison**:
- **On-Demand**: Pay-as-you-go, no commitment
- **Savings Plans**: 1-3 year commitment, ~72% savings
- **Reserved Instances**: Specific instance type, ~75% savings
- **Spot Instances**: Unused capacity, ~90% savings

### Spot Instances

Use Spot Instances for fault-tolerant workloads:

```bash
# Launch Spot Instance
aws ec2 run-instances \
    --image-id ami-12345678 \
    --instance-type c5.large \
    --instance-market-options 'MarketType=spot,SpotOptions={MaxPrice=0.05}'
```

**Best Practices**:
- Use for stateless workloads
- Implement graceful shutdown handling
- Use Spot Fleet for diversity
- Mix Spot and On-Demand instances

### Auto Scaling

Automatically adjust capacity:

```yaml
AutoScalingGroup:
  Type: AWS::AutoScaling::AutoScalingGroup
  Properties:
    MinSize: 2
    MaxSize: 10
    DesiredCapacity: 4
    TargetTrackingScalingPolicies:
      - TargetValue: 70.0
        PredefinedMetricSpecification:
          PredefinedMetricType: ASGAverageCPUUtilization
```

## High Availability

### Multi-AZ Deployment

Deploy across multiple Availability Zones:

```
Region (us-east-1)
├── AZ 1 (us-east-1a)
│   └── OSv Instance 1
├── AZ 2 (us-east-1b)
│   └── OSv Instance 2
└── AZ 3 (us-east-1c)
    └── OSv Instance 3
```

### Elastic Load Balancing

Distribute traffic across instances:

```yaml
ApplicationLoadBalancer:
  Type: AWS::ElasticLoadBalancingV2::LoadBalancer
  Properties:
    Scheme: internet-facing
    Subnets:
      - !Ref PublicSubnet1
      - !Ref PublicSubnet2
    SecurityGroups:
      - !Ref LoadBalancerSecurityGroup

TargetGroup:
  Type: AWS::ElasticLoadBalancingV2::TargetGroup
  Properties:
    Port: 8080
    Protocol: HTTP
    VpcId: !Ref VPC
    HealthCheckPath: /health
    HealthCheckIntervalSeconds: 30
```

**Load Balancer Types**:
- **Application Load Balancer (ALB)**: HTTP/HTTPS, Layer 7
- **Network Load Balancer (NLB)**: TCP/UDP, Layer 4, ultra-low latency
- **Gateway Load Balancer (GWLB)**: Network virtual appliances

### Amazon RDS for Data Persistence

Use managed database services:

```yaml
DatabaseInstance:
  Type: AWS::RDS::DBInstance
  Properties:
    Engine: postgres
    EngineVersion: 14.7
    DBInstanceClass: db.t3.medium
    AllocatedStorage: 100
    MultiAZ: true  # High availability
    BackupRetentionPeriod: 7
    PreferredBackupWindow: 03:00-04:00
```

### Amazon Route 53

Implement DNS-based routing and failover:

```yaml
HealthCheck:
  Type: AWS::Route53::HealthCheck
  Properties:
    Type: HTTPS
    ResourcePath: /health
    FullyQualifiedDomainName: app.example.com
    Port: 443

RecordSet:
  Type: AWS::Route53::RecordSet
  Properties:
    HostedZoneId: !Ref HostedZone
    Name: app.example.com
    Type: A
    SetIdentifier: Primary
    Failover: PRIMARY
    HealthCheckId: !Ref HealthCheck
    AliasTarget:
      DNSName: !GetAtt LoadBalancer.DNSName
      HostedZoneId: !GetAtt LoadBalancer.CanonicalHostedZoneID
```

## Security Best Practices

### Encryption

**Data at Rest**:
- Enable EBS encryption
- Use AWS KMS for key management
- Encrypt S3 buckets

**Data in Transit**:
- Use TLS/SSL for all connections
- Enable HTTPS on load balancers
- Use VPN or Direct Connect for on-premises connectivity

### Compliance

**AWS Services for Compliance**:
- **AWS Config**: Track resource configurations
- **AWS CloudTrail**: Audit API calls
- **AWS Security Hub**: Centralized security findings
- **AWS GuardDuty**: Threat detection

### Network Security

**Best Practices**:
- Use AWS WAF for web application protection
- Enable AWS Shield for DDoS protection
- Implement VPC endpoints for AWS services
- Use Security Groups as stateful firewalls

## Additional Resources

### AWS Documentation
- [Amazon EC2 User Guide](https://docs.aws.amazon.com/ec2/)
- [AWS Lambda Developer Guide](https://docs.aws.amazon.com/lambda/)
- [Amazon VPC User Guide](https://docs.aws.amazon.com/vpc/)
- [AWS Well-Architected Framework](https://aws.amazon.com/architecture/well-architected/)

### OSv on AWS
- [Running OSv on AWS EC2](https://github.com/cloudius-systems/osv/wiki/Running-OSv-on-AWS-EC2)
- [Firecracker Integration](https://github.com/cloudius-systems/osv/wiki/Running-OSv-on-Firecracker)

### AWS Best Practices
- [AWS Security Best Practices](https://aws.amazon.com/architecture/security-identity-compliance/)
- [Cost Optimization Pillar](https://docs.aws.amazon.com/wellarchitected/latest/cost-optimization-pillar/)
- [Performance Efficiency Pillar](https://docs.aws.amazon.com/wellarchitected/latest/performance-efficiency-pillar/)

Last Updated: 2025-12-10
